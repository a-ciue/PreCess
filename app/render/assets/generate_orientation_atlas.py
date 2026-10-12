"""离线生成坐标系按钮图集；仅维护资源时需要 Pillow，运行时直接加载 PNG。"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

SCALE = 3
CELL = 28
COLORS = [(245, 71, 97), (143, 199, 51), (61, 140, 242)]
FONT = ImageFont.truetype("C:/Windows/Fonts/seguisb.ttf", 13 * SCALE)
atlas = Image.new("RGBA", (CELL * SCALE * 12, CELL * SCALE))

for axis, color in enumerate(COLORS):
    for direction in range(2):
        for hovered in range(2):
            image = Image.new("RGBA", (CELL * SCALE, CELL * SCALE))
            draw = ImageDraw.Draw(image)
            radius = (10 if direction == 0 else 8) * SCALE
            center = CELL * SCALE / 2
            bounds = (center - radius, center - radius, center + radius, center + radius)
            if direction == 0 or hovered:
                fill = (*color, 255)
                outline = (255, 255, 255, 235) if hovered else (*[int(c * 0.87) for c in color], 255)
            else:
                fill = (*[int(c * 0.48) for c in color], 70)
                outline = (*color, 255)
            draw.ellipse(bounds, fill=fill, outline=outline, width=round((1.5 if hovered else 1) * SCALE))
            if direction == 0 or hovered:
                label = ("" if direction == 0 else "−") + "XYZ"[axis]
                box = draw.textbbox((0, 0), label, font=FONT)
                origin = (center - (box[2] + box[0]) / 2, center - (box[3] + box[1]) / 2)
                draw.text(origin, label, font=FONT, fill=(16, 27, 40, 255))
            column = axis * 4 + direction * 2 + hovered
            atlas.paste(image, (column * CELL * SCALE, 0))

atlas.save(Path(__file__).with_name("orientation-handles.png"))
background = Image.new("RGBA", (120 * SCALE, 120 * SCALE))
draw = ImageDraw.Draw(background)
draw.ellipse((6 * SCALE, 6 * SCALE, 114 * SCALE, 114 * SCALE),
             fill=(22, 29, 40, 28), outline=(255, 255, 255, 24), width=SCALE)
background.save(Path(__file__).with_name("orientation-hover.png"))
