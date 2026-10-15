import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_photoviewer_icon(output_ico_path, output_png_path=None):
    canvas_size = 1024
    img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Frame boundaries (smooth squircle / rounded card)
    left = 130
    top = 130
    right = 894
    bottom = 894
    radius = 180

    # 1. Soft multi-layered natural drop shadow
    shadow_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    s_draw = ImageDraw.Draw(shadow_img)
    s_draw.rounded_rectangle(
        [left + 16, top + 36, right + 16, bottom + 36],
        radius=radius,
        fill=(0, 0, 0, 70)
    )
    s_draw.rounded_rectangle(
        [left + 8, top + 18, right + 8, bottom + 18],
        radius=radius,
        fill=(0, 0, 0, 90)
    )
    shadow_img = shadow_img.filter(ImageFilter.GaussianBlur(radius=32))
    img.alpha_composite(shadow_img)

    # 2. Main Body Card - Deep Obsidian / Midnight Blue gradient
    card_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    c_draw = ImageDraw.Draw(card_img)

    # Draw rounded card with vertical gradient
    for y in range(top, bottom):
        t = (y - top) / (bottom - top)
        # Deep Indigo (24, 28, 48) to Royal Blue (15, 23, 42)
        r = int(30 * (1 - t) + 15 * t)
        g = int(41 * (1 - t) + 23 * t)
        b = int(68 * (1 - t) + 42 * t)
        c_draw.line([(left, y), (right, y)], fill=(r, g, b, 255))

    # Mask with rounded rectangle
    mask = Image.new("L", (canvas_size, canvas_size), 0)
    m_draw = ImageDraw.Draw(mask)
    m_draw.rounded_rectangle([left, top, right, bottom], radius=radius, fill=255)
    
    card_img.putalpha(mask)
    img.alpha_composite(card_img)

    # Outer border ring (subtle glass highlight)
    draw.rounded_rectangle(
        [left, top, right, bottom],
        radius=radius,
        outline=(255, 255, 255, 35),
        width=4
    )

    # 3. Inner Photo Scene (Vibrant Sun & Mountain Peaks / Aperture)
    # Inner viewport bounds
    v_left = left + 48
    v_top = top + 48
    v_right = right - 48
    v_bottom = bottom - 48
    v_radius = radius - 32

    view_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    v_draw = ImageDraw.Draw(view_img)

    # Sky gradient: Sunset Violet to Warm Coral Orange
    for y in range(v_top, v_bottom):
        t = (y - v_top) / (v_bottom - v_top)
        # Deep Violet (99, 102, 241) -> Hot Coral (244, 63, 94) -> Golden Amber (245, 158, 11)
        if t < 0.5:
            local_t = t * 2.0
            r = int(99 * (1 - local_t) + 244 * local_t)
            g = int(102 * (1 - local_t) + 63 * local_t)
            b = int(241 * (1 - local_t) + 94 * local_t)
        else:
            local_t = (t - 0.5) * 2.0
            r = int(244 * (1 - local_t) + 251 * local_t)
            g = int(63 * (1 - local_t) + 146 * local_t)
            b = int(94 * (1 - local_t) + 60 * local_t)
        v_draw.line([(v_left, y), (v_right, y)], fill=(r, g, b, 255))

    # Glowing Sun
    sun_cx = v_left + 180
    sun_cy = v_top + 180
    sun_r = 75
    # Sun glow
    sun_glow = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    sg_draw = ImageDraw.Draw(sun_glow)
    sg_draw.ellipse([sun_cx - sun_r - 20, sun_cy - sun_r - 20, sun_cx + sun_r + 20, sun_cy + sun_r + 20], fill=(254, 240, 138, 100))
    sun_glow = sun_glow.filter(ImageFilter.GaussianBlur(radius=16))
    view_img.alpha_composite(sun_glow)
    # Sun core
    v_draw.ellipse([sun_cx - sun_r, sun_cy - sun_r, sun_cx + sun_r, sun_cy + sun_r], fill=(255, 255, 255, 245))

    # Mountains: Back Mountain (deep plum)
    v_draw.polygon([
        (v_left + 40, v_bottom),
        (v_left + 320, v_top + 340),
        (v_left + 540, v_bottom)
    ], fill=(76, 29, 149, 210))

    # Foreground Mountains (Royal Dark Navy)
    v_draw.polygon([
        (v_left + 220, v_bottom),
        (v_left + 500, v_top + 280),
        (v_right, v_bottom)
    ], fill=(15, 23, 42, 255))

    # Mountain peak snow cap highlight
    v_draw.polygon([
        (v_left + 500, v_top + 280),
        (v_left + 465, v_top + 335),
        (v_left + 500, v_top + 325),
        (v_left + 535, v_top + 335)
    ], fill=(241, 245, 249, 240))

    # Mask inner viewport
    v_mask = Image.new("L", (canvas_size, canvas_size), 0)
    vm_draw = ImageDraw.Draw(v_mask)
    vm_draw.rounded_rectangle([v_left, v_top, v_right, v_bottom], radius=v_radius, fill=255)
    view_img.putalpha(v_mask)
    img.alpha_composite(view_img)

    # Inner border
    draw.rounded_rectangle(
        [v_left, v_top, v_right, v_bottom],
        radius=v_radius,
        outline=(255, 255, 255, 50),
        width=3
    )

    # 4. Modern Camera Aperture / Shutter / Sparkle Badge in bottom-right corner
    badge_cx = right - 80
    badge_cy = bottom - 80
    badge_r = 100

    # Badge shadow
    b_shadow = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    bs_draw = ImageDraw.Draw(b_shadow)
    bs_draw.ellipse([badge_cx - badge_r + 4, badge_cy - badge_r + 12, badge_cx + badge_r + 4, badge_cy + badge_r + 12], fill=(0, 0, 0, 110))
    b_shadow = b_shadow.filter(ImageFilter.GaussianBlur(radius=12))
    img.alpha_composite(b_shadow)

    # Badge circle (electric cyan / turquoise gradient)
    b_img = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    b_draw = ImageDraw.Draw(b_img)
    b_draw.ellipse([badge_cx - badge_r, badge_cy - badge_r, badge_cx + badge_r, badge_cy + badge_r], fill=(6, 182, 212, 255))
    # Outline
    b_draw.ellipse([badge_cx - badge_r, badge_cy - badge_r, badge_cx + badge_r, badge_cy + badge_r], outline=(255, 255, 255, 220), width=6)
    
    # Lightning bolt / Flash symbol inside badge (representing ultra-fast speed)
    bolt_pts = [
        (badge_cx + 10, badge_cy - 55),
        (badge_cx - 30, badge_cy + 5),
        (badge_cx + 5,  badge_cy + 5),
        (badge_cx - 10, badge_cy + 55),
        (badge_cx + 35, badge_cy - 5),
        (badge_cx,      badge_cy - 5)
    ]
    b_draw.polygon(bolt_pts, fill=(255, 255, 255, 255))
    img.alpha_composite(b_img)

    # Export multi-resolution ICO (256, 128, 64, 48, 32, 24, 16)
    sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
    icon_images = []
    for s in sizes:
        resized = img.resize(s, Image.Resampling.LANCZOS)
        icon_images.append(resized)

    # Save .ico
    os.makedirs(os.path.dirname(os.path.abspath(output_ico_path)), exist_ok=True)
    icon_images[0].save(
        output_ico_path,
        format="ICO",
        sizes=sizes,
        append_images=icon_images[1:]
    )
    print(f"Saved icon to: {output_ico_path}")

    if output_png_path:
        img.resize((512, 512), Image.Resampling.LANCZOS).save(output_png_path, format="PNG")
        print(f"Saved preview PNG to: {output_png_path}")

if __name__ == "__main__":
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ico_dest = os.path.join(base_dir, "resources", "app.ico")
    png_dest = os.path.join(base_dir, "resources", "app_icon_preview.png")
    create_photoviewer_icon(ico_dest, png_dest)
