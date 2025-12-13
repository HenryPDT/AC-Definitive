import os
import zipfile
from pathlib import Path

# --- Configuration ---

# Path to the compiled Release binaries relative to this script
BUILD_DIR = Path("build/Win32/Release")
PLUGINS_SUBDIR = "plugins"

# Output folder for the zips
OUTPUT_DIR = Path("Releases")

# The Core Loader
LOADER_BIN = "PluginLoader.asi"

# Map: ZipName -> List of specific plugins to include
# The script will look for these inside build/Win32/Release/plugins/
GAMES = {
    "AC1-Definitive": [
        "AC1-EaglePatch.asi"
    ],
    "AC2-Definitive": [
        "AC2-EaglePatch.asi",
        # "AC2-ParkourMod.asi" # Uncomment to include ParkourMod
    ],
    "ACB-Definitive": [
        "ACB-EaglePatch.asi",
        # "ACB-ParkourMod.asi" # Uncomment to include ParkourMod
    ],
    "ACR-Definitive": [
        "ACR-EaglePatch.asi"
    ]
}

def main():
    # 1. Verification
    if not BUILD_DIR.exists():
        print(f"[!] Error: Build directory not found: {BUILD_DIR.absolute()}")
        return

    loader_path = BUILD_DIR / LOADER_BIN
    if not loader_path.exists():
        print(f"[!] Error: {LOADER_BIN} not found in {BUILD_DIR}")
        return

    # 2. Create Output Directory
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # 3. Process each game package
    for zip_name, plugins in GAMES.items():
        zip_filename = OUTPUT_DIR / f"{zip_name}.zip"
        print(f"Packaging {zip_filename}...")

        with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zf:
            
            # A. Add PluginLoader.asi -> scripts/PluginLoader.asi
            # arcname is the name inside the zip
            zf.write(loader_path, arcname=f"scripts/{LOADER_BIN}")
            print(f"  + Added {LOADER_BIN}")

            # B. Add specific plugins -> scripts/plugins/Name.asi
            for plugin in plugins:
                plugin_path = BUILD_DIR / PLUGINS_SUBDIR / plugin
                
                if plugin_path.exists():
                    zf.write(plugin_path, arcname=f"scripts/plugins/{plugin}")
                    print(f"  + Added {plugin}")
                else:
                    print(f"  [!] WARNING: Could not find plugin {plugin}")

    print("\n------------------------------------------------")
    print(f"Success! Release zips created in: {OUTPUT_DIR.absolute()}")

if __name__ == "__main__":
    main()
