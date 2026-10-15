import os
from PIL import Image, ImageDraw

def generate_test_images(test_dir):
    os.makedirs(test_dir, exist_ok=True)

    # 1. High-Res PNG (1920x1080)
    img_png = Image.new("RGBA", (1920, 1080), (15, 23, 42, 255))
    draw = ImageDraw.Draw(img_png)
    for i in range(0, 1920, 80):
        draw.line([(i, 0), (i, 1080)], fill=(30, 41, 59, 255), width=1)
    for j in range(0, 1080, 80):
        draw.line([(0, j), (1920, j)], fill=(30, 41, 59, 255), width=1)
    draw.ellipse([760, 340, 1160, 740], fill=(6, 182, 212, 255), outline=(255, 255, 255, 255), width=4)
    png_path = os.path.join(test_dir, "01_highres.png")
    img_png.save(png_path, "PNG")
    print(f"Created: {png_path}")

    # 2. JPEG Photo (1280x720)
    img_jpg = Image.new("RGB", (1280, 720), (245, 158, 11))
    draw_jpg = ImageDraw.Draw(img_jpg)
    draw_jpg.polygon([(200, 600), (640, 200), (1080, 600)], fill=(225, 29, 72))
    jpg_path = os.path.join(test_dir, "02_sunset.jpg")
    img_jpg.save(jpg_path, "JPEG", quality=95)
    print(f"Created: {jpg_path}")

    # 3. Multi-frame Animated GIF (400x400, 8 frames)
    frames = []
    for f in range(8):
        f_img = Image.new("RGBA", (400, 400), (18, 18, 20, 255))
        f_draw = ImageDraw.Draw(f_img)
        angle = f * (360 / 8)
        rad = 120
        import math
        cx = 200 + rad * math.cos(math.radians(angle))
        cy = 200 + rad * math.sin(math.radians(angle))
        f_draw.ellipse([cx - 40, cy - 40, cx + 40, cy + 40], fill=(16, 185, 129, 255))
        frames.append(f_img)

    gif_path = os.path.join(test_dir, "03_animated_spinner.gif")
    frames[0].save(
        gif_path,
        save_all=True,
        append_images=frames[1:],
        duration=100,
        loop=0
    )
    print(f"Created: {gif_path}")

    # 4. WebP Image (800x600)
    img_webp = Image.new("RGBA", (800, 600), (79, 70, 229, 255))
    draw_w = ImageDraw.Draw(img_webp)
    draw_w.rounded_rectangle([100, 100, 700, 500], radius=40, fill=(236, 72, 153, 255))
    webp_path = os.path.join(test_dir, "04_modern.webp")
    img_webp.save(webp_path, "WEBP")
    print(f"Created: {webp_path}")

    # 5. BMP Image (640x480)
    img_bmp = Image.new("RGB", (640, 480), (14, 165, 233))
    draw_b = ImageDraw.Draw(img_bmp)
    draw_b.rectangle([160, 120, 480, 360], fill=(255, 255, 255))
    bmp_path = os.path.join(test_dir, "05_classic.bmp")
    img_bmp.save(bmp_path, "BMP")
    print(f"Created: {bmp_path}")

if __name__ == "__main__":
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    test_folder = os.path.join(base, "samples")
    generate_test_images(test_folder)
