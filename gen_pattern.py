import os
import argparse
import torch
import numpy as np
from PIL import Image, ImageChops, ImageFilter
from diffusers import StableDiffusionPipeline, EulerAncestralDiscreteScheduler

# ------------------- 고급 무이음(PSD) -------------------
def make_seamless_psd(img: Image.Image) -> Image.Image:
    rgb = img.convert("RGB")
    arr = np.asarray(rgb).astype(np.float32)
    h, w, c = arr.shape

    v = np.zeros_like(arr, dtype=np.float32)
    v[0, :, :]  = arr[0, :, :]  - arr[-1, :, :]
    v[-1, :, :] = -v[0, :, :]
    v[:, 0, :]  += arr[:, 0, :] - arr[:, -1, :]
    v[:, -1, :] -= arr[:, 0, :] - arr[:, -1, :]

    V = np.fft.fft2(v, axes=(0, 1))
    yy, xx = np.meshgrid(np.arange(h), np.arange(w), indexing="ij")
    denom = (2*np.cos(2*np.pi*xx/w) + 2*np.cos(2*np.pi*yy/h) - 4).astype(np.float32)
    denom[0, 0] = 1.0
    S = V / denom[..., None]
    S[0, 0, :] = 0.0

    s = np.real(np.fft.ifft2(S, axes=(0, 1))).astype(np.float32)
    p = arr - s
    p = np.clip(p, 0, 255).astype(np.uint8)
    return Image.fromarray(p, mode="RGB")

# ------------------- 간단 무이음(크로스-블렌딩) -------------------
def make_seamless(img: Image.Image, feather: int = 18) -> Image.Image:
    img = img.convert("RGBA")
    w, h = img.size
    off = ImageChops.offset(img, w // 2, h // 2)

    y, x = np.ogrid[:h, :w]
    cx, cy = w // 2, h // 2
    mv = np.clip(feather - np.abs(x - cx), 0, feather)
    mh = np.clip(feather - np.abs(y - cy), 0, feather)
    mask = np.maximum(mv, mh)
    if mask.max() == 0:
        return off
    mask = (mask / mask.max() * 255).astype("uint8")
    mask = Image.fromarray(mask, mode="L").filter(ImageFilter.GaussianBlur(feather))
    blurred = off.filter(ImageFilter.GaussianBlur(feather))
    return Image.composite(blurred, off, mask).convert("RGBA")

# ------------------- 평면 알베도화(팔레트 양자화) -------------------
def flatten_to_albedo(img: Image.Image, colors: int = 8, dither: bool = False, blur: int = 0) -> Image.Image:
    dither_mode = Image.FLOYDSTEINBERG if dither else Image.NONE
    q = img.convert("RGB").quantize(colors=colors, method=Image.Quantize.FASTOCTREE, dither=dither_mode)
    out = q.convert("RGB")
    if blur > 0:
        out = out.filter(ImageFilter.GaussianBlur(blur))
    return out

# ------------------- CLIP 77-token safe truncate -------------------
def clip_truncate(pipe, text: str) -> str:
    tok = pipe.tokenizer
    enc = tok(text, truncation=True, max_length=77,
              add_special_tokens=True, return_overflowing_tokens=False)
    trimmed = tok.decode(enc["input_ids"], skip_special_tokens=True)
    return " ".join(trimmed.split())

# ------------------- 메인 -------------------
def main():
    DEFAULT_STYLE = (
        ""
    )
    BASE_FALLBACK = (
        ""
    )

    ap = argparse.ArgumentParser()
    ap.add_argument("prompt", nargs="*", default=[], help="사용자 프롬프트")
    ap.add_argument("--model", default="sd-legacy/stable-diffusion-v1-5")
    ap.add_argument("--size", type=int, default=1024)
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--guidance", type=float, default=6.8)
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--seamless", action="store_true")
    ap.add_argument("--seamless-psd", action="store_true")
    ap.add_argument("--flat-post", action="store_true")
    ap.add_argument("--flat-colors", type=int, default=8)
    ap.add_argument("--flat-dither", action="store_true")
    ap.add_argument("--rgb", action="store_true")
    ap.add_argument("--out", default="textures/generated.png")
    ap.add_argument("--style", default=DEFAULT_STYLE)
    ap.add_argument("--style-off", action="store_true")
    ap.add_argument("--neg", default=(
        "photorealistic, 3d, global lighting, drop shadow, outer glow, highlights, "
        "bevel, emboss, vignette, reflection, normal map, bump, displacement, "
        "noisy, blur, text, logo, watermark"
    ))
    args = ap.parse_args()

    user = " ".join(args.prompt).strip()
    if args.style_off:
        final_prompt = user or BASE_FALLBACK
    else:
        final_prompt = f"{user}, {args.style}" if user else BASE_FALLBACK

    device = "cuda" if torch.cuda.is_available() else "cpu"
    dtype  = torch.float16 if device == "cuda" else torch.float32

    # 파이프라인
    pipe = StableDiffusionPipeline.from_pretrained(args.model, torch_dtype=dtype)
    pipe = pipe.to(device)
    pipe.enable_attention_slicing()
    pipe.enable_vae_slicing()
    try:
        import xformers
        pipe.enable_xformers_memory_efficient_attention()
    except Exception:
        pass
    try:
        pipe.scheduler = EulerAncestralDiscreteScheduler.from_config(pipe.scheduler.config)
    except Exception:
        pass

    generator = None
    if args.seed is not None:
        generator = torch.Generator(device=device).manual_seed(args.seed)

    # --- 여기서 토큰 잘라줌 ---
    final_prompt = clip_truncate(pipe, final_prompt)
    args.neg     = clip_truncate(pipe, args.neg)

    # 생성 (OOM 시 768 폴백)
    try:
        img = pipe(
            final_prompt,
            negative_prompt=args.neg,
            width=args.size, height=args.size,
            num_inference_steps=args.steps,
            guidance_scale=args.guidance,
            generator=generator
        ).images[0]
    except RuntimeError as e:
        if "out of memory" in str(e).lower() and args.size > 768 and device == "cuda":
            torch.cuda.empty_cache()
            print("⚠️ OOM 발생: size=768로 자동 재시도합니다.")
            img = pipe(
                final_prompt,
                negative_prompt=args.neg,
                width=768, height=768,
                num_inference_steps=args.steps,
                guidance_scale=args.guidance,
                generator=generator
            ).images[0]
        else:
            raise

    # 후처리
    if args.seamless_psd:
        img = make_seamless_psd(img)
    if args.seamless:
        img = make_seamless(img)
    if args.flat_post:
        img = flatten_to_albedo(img, colors=args.flat_colors, dither=args.flat_dither)
    if args.rgb:
        img = img.convert("RGB")

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    img.save(args.out)
    print(f"✅ Saved: {args.out}")
    print(f"🎯 Prompt used (<=77t): {final_prompt}")
    print(f"🙅 Negative (<=77t): {args.neg}")

if __name__ == "__main__":
    main()
