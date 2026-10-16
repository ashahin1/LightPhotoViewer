import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_photoviewer_icon(output_ico_path, output_png_path=None):
    # Render at high resolution (1024x1024) for supersampled, razor-sharp anti-aliasing
    canvas_size = 1024
    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))

    # Outer photo card frame
    left = 130
    top = 130
    right = 894
    bottom = 894
    radius = 180

    # 1. Multi-stage soft realistic drop shadow (designed for both light & dark explorer backgrounds)
    shadow_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow_img)
    # Deep ambient contact shadow
    s_draw.rounded_rectangle(
        [left + 10, top + 36, right + 10, bottom + 36],
        radius=radius,
        fill=(15, 23, 42, 45)
    )
    # Soft spread shadow
    s_draw.rounded_rectangle(
        [left + 4, top + 18, right + 4, bottom + 18],
        radius=radius,
        fill=(15, 23, 42, 60)
    )
    shadow_img = shadow_img.filter(ImageFilter.GaussianBlur(radius=28))
    img.alpha_composite(shadow_img)

    # 2. Main Outer Card - Crisp, Pure Light / White Porcelain Card
    card_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    c_draw = ImageDraw.Draw(card_img)

    # Subtle top-to-bottom light gradient (Pure white to soft warm alabaster #F8FAFC)
    for y in range(top, bottom):
        t = (y - top) / (bottom - top)
        # 255 -> 248
        val = int(255 * (1 - t) + 246 * t)
        c_draw.line([(left, y), (right, y)], fill=(val, val, val + 2, 255))

    card_mask = Image.new("L", (canvas_size, canvas_size), 0)
    cm_draw = ImageDraw.Draw(card_mask)
    cm_draw.rounded_rectangle([left, top, right, bottom], radius=radius, fill=255)
    card_img.putalpha(card_mask)
    img.alpha_composite(card_img)

    # Crisp card outline (subtle light-slate border so it stands out distinctly on pure white surfaces)
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        [left, top, right, bottom],
        radius=radius,
        outline=(203, 213, 225, 255), # Slate-300
        width=5
    )

    # 3. Inner Photo Viewport (The vibrant landscape picture)
    pad = 52
    v_left = left + pad
    v_top = top + pad
    v_right = right - pad
    v_bottom = bottom - pad
    v_radius = radius - 36

    view_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    v_draw = ImageDraw.Draw(view_img)

    # Sky gradient: Bright Azure Sky (Cyan-500 -> Sky-400 -> Warm Amber Horizon)
    for y in range(v_top, v_bottom):
        t = (y - v_top) / (v_bottom - v_top)
        if t < 0.55:
            # Azure / Deep Sky Blue (#0284C7 -> #38BDF8)
            local_t = t / 0.55
            r = int(2 * (1 - local_t) + 56 * local_t)
            g = int(132 * (1 - local_t) + 189 * local_t)
            b = int(199 * (1 - local_t) + 248 * local_t)
        else:
            # Warm glowing horizon (#38BDF8 -> #FDE68A)
            local_t = (t - 0.55) / 0.45
            r = int(56 * (1 - local_t) + 253 * local_t)
            g = int(189 * (1 - local_t) + 230 * local_t)
            b = int(248 * (1 - local_t) + 138 * local_t)
        v_draw.line([(v_left, y), (v_right, y)], fill=(r, g, b, 255))

    # Radiant Golden Sun
    sun_cx = v_left + 175
    sun_cy = v_top + 165
    sun_r = 72

    # Sun Outer Glow
    sun_glow = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    sg_draw = ImageDraw.Draw(sun_glow)
    sg_draw.ellipse([sun_cx - sun_r - 28, sun_cy - sun_r - 28, sun_cx + sun_r + 28, sun_cy + sun_r + 28], fill=(254, 240, 138, 120))
    sun_glow = sun_glow.filter(ImageFilter.GaussianBlur(radius=20))
    view_img.alpha_composite(sun_glow)

    # Sun Core
    v_draw.ellipse([sun_cx - sun_r, sun_cy - sun_r, sun_cx + sun_r, sun_cy + sun_r], fill=(255, 251, 235, 255))
    v_draw.ellipse([sun_cx - sun_r + 8, sun_cy - sun_r + 8, sun_cx + sun_r - 8, sun_cy + sun_r - 8], fill=(254, 215, 170, 240))

    # Mountain Layers:
    # 1. Distant Mountain Range (Soft Lavender/Indigo #818CF8)
    v_draw.polygon([
        (v_left + 30, v_bottom),
        (v_left + 290, v_top + 310),
        (v_left + 530, v_bottom)
    ], fill=(99, 102, 241, 230))

    # 2. Midground Mountain Range (Rich Teal/Cyan #0F766E)
    v_draw.polygon([
        (v_left + 180, v_bottom),
        (v_left + 460, v_top + 250),
        (v_right, v_bottom)
    ], fill=(13, 148, 136, 255))

    # Crisp Snowcap on Midground Peak
    v_draw.polygon([
        (v_left + 460, v_top + 250),
        (v_left + 425, v_top + 305),
        (v_left + 460, v_top + 295),
        (v_left + 495, v_top + 305)
    ], fill=(255, 255, 255, 255))

    # 3. Foreground Hills (Fresh Emerald Green #059669 / #10B981)
    v_draw.polygon([
        (v_left - 10, v_bottom),
        (v_left + 160, v_top + 430),
        (v_left + 380, v_bottom)
    ], fill=(5, 150, 105, 255))

    # Inner Viewport Mask
    v_mask = Image.new("L", (canvas_size, canvas_size), 0)
    vm_draw = ImageDraw.Draw(v_mask)
    vm_draw.rounded_rectangle([v_left, v_top, v_right, v_bottom], radius=v_radius, fill=255)
    view_img.putalpha(v_mask)
    img.alpha_composite(view_img)

    # Inner Viewport crisp outline
    draw.rounded_rectangle(
        [v_left, v_top, v_right, v_bottom],
        radius=v_radius,
        outline=(226, 232, 240, 255),
        width=3
    )

    # 4. Badge: Fast Lighting Aperture Emblem on Bottom-Right
    badge_cx = right - 76
    badge_cy = bottom - 76
    badge_r = 96

    # Badge Drop Shadow
    b_shadow = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    bs_draw = ImageDraw.Draw(b_shadow)
    bs_draw.ellipse([badge_cx - badge_r + 4, badge_cy - badge_r + 10, badge_cx + badge_r + 4, badge_cy + badge_r + 10], fill=(15, 23, 42, 90))
    b_shadow = b_shadow.filter(ImageFilter.GaussianBlur(radius=14))
    img.alpha_composite(b_shadow)

    # Badge Circle - Vibrant Electric Cyan / Sky Blue Gradient
    b_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    b_draw = ImageDraw.Draw(b_img)
    b_draw.ellipse([badge_cx - badge_r, badge_cy - badge_r, badge_cx + badge_r, badge_cy + badge_r], fill=(2, 132, 199, 255))
    # Inner gradient glow
    b_draw.ellipse([badge_cx - badge_r + 4, badge_cy - badge_r + 4, badge_cx + badge_r - 4, badge_cy + badge_r - 4], fill=(6, 182, 212, 255))
    # Clean white rim
    b_draw.ellipse([badge_cx - badge_r, badge_cy - badge_r, badge_cx + badge_r, badge_cy + badge_r], outline=(255, 255, 255, 255), width=6)

    # Crisp Lightning Bolt (Speed Symbol)
    bolt_pts = [
        (badge_cx + 10, badge_cy - 52),
        (badge_cx - 28, badge_cy + 4),
        (badge_cx + 4,  badge_cy + 4),
        (badge_cx - 10, badge_cy + 52),
        (badge_cx + 32, badge_cy - 4),
        (badge_cx,      badge_cy - 4)
    ]
    b_draw.polygon(bolt_pts, fill=(255, 255, 255, 255))
    img.alpha_composite(b_img)

    # 5. Comprehensive resolutions for Windows File Explorer
    # To prevent pixelation at EVERY possible explorer preset and DPI scale:
    # 256 (Extra Large / 4K), 128 (Large at high DPI), 96 (Explorer Large 100%), 64 (Tiles 150%),
    # 48 (Explorer Medium), 40 (125%), 32 (Explorer Small / Details), 24 (150% taskbar), 20 (125% taskbar), 16 (100% taskbar)
    target_sizes = [
        (256, 256),
        (128, 128),
        (96, 96),
        (64, 64),
        (48, 48),
        (40, 40),
        (32, 32),
        (24, 24),
        (20, 20),
        (16, 16)
    ]

    icon_images = []
    for s in target_sizes:
        # High-quality Lanczos downsampling with sharp details
        resized = img.resize(s, Image.Resampling.LANCZOS)
        icon_images.append(resized)

    os.makedirs(os.path.dirname(os.path.abspath(output_ico_path)), exist_ok=True)

    # Save as multi-resolution ICO with 256x256 as PNG
    icon_images[0].save(
        output_ico_path,
        format="ICO",
        sizes=target_sizes,
        append_images=icon_images[1:]
    )
    print(f"Generated Light-Themed multi-resolution ICO ({len(target_sizes)} sizes): {output_ico_path}")

    if output_png_path:
        img.resize((512, 512), Image.Resampling.LANCZOS).save(output_png_path, format="PNG")
        print(f"Generated Preview PNG: {output_png_path}")

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ico_dest = os.path.join(base_dir, "resources", "app.ico")
    png_dest = os.path.join(base_dir, "resources", "app_icon_preview.png")
    create_photoviewer_icon(ico_dest, png_dest)
