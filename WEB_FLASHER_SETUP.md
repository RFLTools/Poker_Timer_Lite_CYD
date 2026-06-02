# Web Flasher Setup Guide

This guide will help you set up the web-based firmware flasher for your Poker Timer CYD project.

## What is the Web Flasher?

The web flasher allows users to install firmware directly from their web browser without needing to:
- Install PlatformIO or Arduino IDE
- Configure build environments
- Understand COM ports and upload settings
- Deal with driver issues manually

Users simply:
1. Visit your GitHub Pages URL
2. Connect their device via USB
3. Click "Install"
4. Done!

## Quick Setup (5 Minutes)

### Step 1: Enable GitHub Pages

1. Go to your repository on GitHub: https://github.com/RFLTools/Poker_Timer_Lite_CYD
2. Click on "Settings" (top menu)
3. Scroll down to "Pages" in the left sidebar
4. Under "Build and deployment":
   - **Source:** Deploy from a branch
   - **Branch:** gh-pages
   - **Folder:** / (root)
5. Click "Save"

### Step 2: Push Your Changes

```bash
git add .
git commit -m "Add web-based firmware flasher"
git push origin main
```

### Step 3: Wait for Build

The GitHub Actions workflow will:
1. Build the firmware automatically
2. Create the web flasher files
3. Deploy to GitHub Pages

This takes about 3-5 minutes on first run (subsequent builds are faster due to caching).

### Step 4: Access Your Web Flasher

After the workflow completes, your web flasher will be available at:

```
https://rfltools.github.io/Poker_Timer_Lite_CYD/
```

## Monitoring the Build

1. Go to the "Actions" tab in your repository
2. You'll see the "Build and Deploy Web Flasher" workflow running
3. Click on it to see progress
4. Green checkmark = Success!
5. Red X = Something went wrong (click to see logs)

## Troubleshooting

### Workflow Not Running

**Problem:** No workflow appears in Actions tab

**Solution:**
- Make sure you've pushed the `.github/workflows/build.yml` file
- Check that Actions are enabled: Settings > Actions > General > "Allow all actions"

### Build Fails

**Problem:** Workflow shows red X

**Solutions:**
1. Click on the failed workflow to see error logs
2. Common issues:
   - Missing dependencies in `platformio.ini`
   - Syntax errors in code
   - Out of memory (unlikely with this project)

### GitHub Pages Not Working

**Problem:** 404 error when visiting the URL

**Solutions:**
1. Wait a few minutes after the workflow completes
2. Check GitHub Pages settings (Settings > Pages)
3. Verify the `gh-pages` branch exists (it's created automatically)
4. Make sure GitHub Pages is enabled for your repository type (public repos work automatically)

### Can't See Port in Browser

**Problem:** When clicking "Install", no ports appear

**Solutions:**
1. Make sure you're using Chrome, Edge, or Opera (not Firefox/Safari)
2. Device must be physically connected via USB
3. Install the appropriate USB driver:
   - **Windows:** Usually auto-installed, or download from:
     - CP210x: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
     - CH340: http://www.wch-ic.com/downloads/CH341SER_EXE.html
   - **Mac:** May need to install manually
   - **Linux:** Usually works out of the box

## Customization

### Change the URL

The default URL is based on your GitHub username and repository name:
```
https://[username].github.io/[repository-name]/
```

To use a custom domain:
1. Settings > Pages > Custom domain
2. Enter your domain (e.g., `flasher.yourdomain.com`)
3. Configure DNS (see GitHub's documentation)

### Update Firmware Version

Edit `web_flasher/manifest.json`:
```json
{
  "version": "1.0.1"
}
```

Then commit and push. The new version will be built automatically.

### Modify the UI

Edit `web_flasher/index.html` to:
- Change colors and styling
- Update feature descriptions
- Add your logo
- Modify installation instructions

### Manual Testing

To test changes locally before pushing:

```bash
# Build the firmware
pio run --environment esp32-2432s028

# Copy binaries
.\build_web_flasher.ps1  # Windows
./build_web_flasher.sh   # Linux/Mac

# Start web server
cd web_flasher
python -m http.server 8000

# Open in browser
# http://localhost:8000
```

## File Structure

```
web_flasher/
├── index.html          # Web interface (edit freely)
├── manifest.json       # Firmware config (update version here)
├── README.md          # Documentation
├── .gitignore         # Prevents committing binaries
└── *.bin              # Generated files (not in git)
```

## Security Notes

1. **Binary Verification:** GitHub Actions builds from your source code, ensuring the binaries match your code
2. **HTTPS:** GitHub Pages automatically uses HTTPS
3. **Web Serial API:** Requires user interaction (selecting the port)
4. **Open Source:** Users can inspect the code before flashing

## Sharing Your Web Flasher

Share the URL with:
- Your project's README (already done!)
- Social media posts
- Forum threads
- Documentation
- Product packaging

Example markdown badge:
```markdown
[![Flash Firmware](https://img.shields.io/badge/Flash-Firmware-blue?style=for-the-badge)](https://rfltools.github.io/Poker_Timer_Lite_CYD/)
```

Result:
[![Flash Firmware](https://img.shields.io/badge/Flash-Firmware-blue?style=for-the-badge)](https://rfltools.github.io/Poker_Timer_Lite_CYD/)

## Advanced: Multiple Firmware Versions

To offer multiple firmware versions:

1. Modify `manifest.json` to include multiple builds:
```json
{
  "name": "Poker Timer CYD",
  "builds": [
    {
      "chipFamily": "ESP32",
      "name": "Stable",
      "parts": [...]
    },
    {
      "chipFamily": "ESP32",
      "name": "Beta",
      "parts": [...]
    }
  ]
}
```

2. Modify the workflow to build multiple configurations
3. Users will see a dropdown to select which version to install

## Support

If users have issues with the web flasher:
1. Check their browser (must be Chrome/Edge/Opera)
2. Verify USB cable is data-capable (not power-only)
3. Check device drivers are installed
4. Try a different USB port
5. Hold BOOT button while connecting if flash fails

## Next Steps

Once your web flasher is live:
- [ ] Test it yourself with a real device
- [ ] Share the URL with beta testers
- [ ] Add the URL to your project documentation
- [ ] Consider adding it to your GitHub repository description
- [ ] Submit to ESP32 project directories/forums

## Additional Resources

- [ESP Web Tools Documentation](https://esphome.github.io/esp-web-tools/)
- [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)
- [GitHub Pages Documentation](https://docs.github.com/en/pages)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)

---

**Congratulations!** Your web flasher is now set up and ready to make firmware installation effortless for your users! 🎉
