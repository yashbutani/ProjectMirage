import os
import json
import numpy as np
import cv2
from PIL import Image, ImageDraw, ImageFont
import pyte

# --- CONFIGURATION VARIABLES ---
INPUT_FILE = "video_demo.cast"
OUTPUT_FILE = "mirage_demo.mp4"
FPS = 30                 # Frames per second of the output video
PLAYBACK_SPEED = 1.2     # Increase to speed up terminal thinking times (e.g., 1.5)
END_PADDING_SEC = 2.0    # Hold the last frame for a few seconds

def get_char_size(font):
    """Bulletproof character dimension calculations for cross-platform PIL."""
    if hasattr(font, "getbbox"):
        bbox = font.getbbox("A")
        return bbox[2] - bbox[0], bbox[3] - bbox[1]
    elif hasattr(font, "getsize"):
        return font.getsize("A")
    return 8, 14

def select_color(line_text):
    """Automated high-production color theme matching the Project Mirage architecture."""
    text_lower = line_text.lower()
    
    # 1. Core Interception Hooks (Bright Neon Green)
    if "🛡️" in line_text or "[mirage kernel hook]" in text_lower:
        return (50, 255, 100)
    # 2. Kernel Actions & Fabrications (Vibrant Cyber Pink)
    elif "🪄" in line_text or "[mirage action]" in text_lower:
        return (255, 80, 255)
    # 3. Agent Decision Matrices & Path Targets (Orange/Amber Alert)
    elif "🤖" in line_text or "[agent intent]" in text_lower:
        return (255, 165, 0)
    elif "🎯" in line_text or "[target path]" in text_lower:
        return (255, 90, 90)
    # 4. Identity Cues
    elif "[parent]" in text_lower:
        return (130, 170, 255)  # Safe Parent Blueprint Blue
    elif "[gemini]" in text_lower:
        return (180, 140, 255)  # Autonomous Agent Purple
    elif "[agent]" in text_lower:
        return (0, 230, 200)    # Subprocess Mint Teal
    elif "critical" in text_lower or "error" in text_lower or "override" in text_lower:
        return (255, 60, 60)    # Security Alert Red
        
    # Default Text Color (Crisp Terminal Light Gray)
    return (220, 225, 230)

def main():
    if not os.path.exists(INPUT_FILE):
        print(f"❌ Target file '{INPUT_FILE}' not found. Place script in the same directory.")
        return

    print("📖 Step 1: Parsing asciicast log format and metrics...")
    events = []
    cols, rows = 126, 40  # Defaults
    
    with open(INPUT_FILE, "r") as f:
        # Extract metadata from header line
        try:
            header = json.loads(f.readline())
            cols = header.get("width", cols)
            rows = header.get("height", rows)
        except Exception:
            pass
            
        # Extract timeline payloads
        for line in f:
            try:
                item = json.loads(line)
                if len(item) == 3 and item[1] == "o":
                    events.append((item[0], item[2]))
            except json.JSONDecodeError:
                continue

    if not events:
        print("❌ Error: Valid streaming frame event matrices were not detected.")
        return

    max_timestamp = events[-1][0]
    total_duration = max_timestamp + END_PADDING_SEC
    print(f"   ↳ Read {len(events)} discrete events. Session span: {max_timestamp:.2f}s.")

    # Step 2: Initialize virtual terminal and font assets
    screen = pyte.Screen(cols, rows)
    stream = pyte.Stream(screen)
    
    try:
        font = ImageFont.load_default(size=15)
    except (TypeError, AttributeError):
        font = ImageFont.load_default()

    char_w, char_h = get_char_size(font)
    line_spacing = char_h + 4
    
    # Calculate dimensions
    pad = 20
    canvas_w = cols * char_w + (pad * 2)
    canvas_h = rows * line_spacing + (pad * 2)

    # Upscale smaller raster fonts with pixel-perfection for production displays
    target_scale = 2 if canvas_w < 1000 else 1
    out_w = canvas_w * target_scale
    out_h = canvas_h * target_scale

    print(f"🎬 Step 2: Building video engine ({out_w}x{out_h} @ {FPS} FPS)...")
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    video = cv2.VideoWriter(OUTPUT_FILE, fourcc, FPS, (out_w, out_h))

    current_sim_time = 0.0
    dt = 1.0 / FPS
    event_idx = 0
    total_frames = int(total_duration * FPS / PLAYBACK_SPEED)
    
    print("🎨 Step 3: Simulating terminal environment and rasterizing frames...")
    
    try:
        frame_count = 0
        while current_sim_time <= total_duration:
            # Advance simulation time based on configured execution speed modifiers
            scaled_time = current_sim_time * PLAYBACK_SPEED
            
            # Feed data chunks into virtual shell buffer as time crosses the marks
            while event_idx < len(events) and events[event_idx][0] <= scaled_time:
                stream.feed(events[event_idx][1])
                event_idx += 1

            # Render frame canvas
            img = Image.new("RGB", (canvas_w, canvas_h), color=(22, 22, 26)) # Dark obsidian
            draw = ImageDraw.Draw(img)
            
            for r_idx, display_line in enumerate(screen.display):
                y_pos = pad + (r_idx * line_spacing)
                text_color = select_color(display_line)
                draw.text((pad, y_pos), display_line, font=font, fill=text_color)

            # Convert to OpenCV compatible array matrix
            frame_np = np.array(img)
            frame_bgr = cv2.cvtColor(frame_np, cv2.COLOR_RGB2BGR)

            # Apply nearest-neighbor scaling to keep lines perfectly blocky and clear
            if target_scale > 1:
                frame_bgr = cv2.resize(frame_bgr, (out_w, out_h), interpolation=cv2.INTER_NEAREST)

            video.write(frame_bgr)
            current_sim_time += dt
            frame_count += 1
            
            if frame_count % 100 == 0 or current_sim_time >= total_duration:
                pct = min(100, (frame_count / total_frames) * 100)
                print(f"   ↳ Progress: {pct:.1f}% rendered ({frame_count}/{total_frames} frames)...")

    finally:
        video.release()
        print(f"\n🎉 Process Complete! Output saved cleanly to disk: {os.path.abspath(OUTPUT_FILE)}")

if __name__ == "__main__":
    main()