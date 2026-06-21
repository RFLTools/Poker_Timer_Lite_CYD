Import("env")

import shutil
import json
from datetime import datetime
from pathlib import Path

def update_web_flasher(source, target, env):
    """Post-build script to automatically update web flasher files"""
    
    print("\n" + "="*60)
    print("  Updating Web Flasher Files")
    print("="*60)
    
    # Define paths
    build_dir = Path(".pio/build/esp32-2432s028")
    web_flasher_dir = Path("web_flasher")
    
    # Create web_flasher directory if it doesn't exist
    web_flasher_dir.mkdir(exist_ok=True)
    
    # Files to copy with their offsets (for reference in manifest)
    files_to_copy = [
        "firmware.bin",
        "bootloader.bin",
        "partitions.bin"
    ]
    
    # Copy firmware files
    for filename in files_to_copy:
        src = build_dir / filename
        dst = web_flasher_dir / filename
        if src.exists():
            shutil.copy2(src, dst)
            size_kb = dst.stat().st_size / 1024
            print(f"  [OK] {filename} ({size_kb:.2f} KB)")
        else:
            print(f"  [ERROR] {filename} not found!")
    
    # Handle boot_app0.bin
    boot_app0_locations = [
        Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin",
        build_dir / "boot_app0.bin"
    ]
    
    boot_app0_found = False
    for location in boot_app0_locations:
        if location.exists():
            shutil.copy2(location, web_flasher_dir / "boot_app0.bin")
            size_kb = (web_flasher_dir / "boot_app0.bin").stat().st_size / 1024
            print(f"  [OK] boot_app0.bin ({size_kb:.2f} KB)")
            boot_app0_found = True
            break
    
    if not boot_app0_found:
        print("  [WARN] boot_app0.bin not found, creating...")
        with open(web_flasher_dir / "boot_app0.bin", "wb") as f:
            f.write(b'\xff' * 4096)
        print("  [OK] boot_app0.bin created (4.00 KB)")
    
    # Update manifest.json with current build date
    manifest_path = web_flasher_dir / "manifest.json"
    if manifest_path.exists():
        with open(manifest_path, 'r') as f:
            manifest = json.load(f)
        
        # Update build date
        build_date = datetime.now().strftime("%Y-%m-%d")
        manifest['build_date'] = build_date
        
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
        
        print(f"  [OK] Build date set to: {build_date}")
    else:
        print("  [ERROR] manifest.json not found!")
    
    print("="*60)
    print("  Web flasher files updated successfully!")
    print("="*60 + "\n")

# Register the post-build action
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", update_web_flasher)
