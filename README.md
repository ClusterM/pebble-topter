# TOTPer - TOTP Authenticator for Pebble

A powerful, standalone Time-based One-Time Password (TOTP) authenticator for Pebble smartwatches that doesn't require a companion app on your phone.


## ⚠️ DISCLAIMER

TOTPer should not be your only authenticator. Why? I cannot take responsibility if you lose access to your accounts :) Please use TOTPer as a secondary authenticator. It’s easy to add your account keys to multiple authenticators at once: simply scan the same QR code with both TOTPer and your primary app (such as Google Authenticator, Authy, etc.).


## Features
- **No Companion App Required**: TOTPer uses a local HTML page for settings
- **Fast Loading**: Optimized for quick startup and instant code generation
- **QR Code Parsing**: Paste QR code URLs in the phone configuration page (Pebble app has no camera access)
- **Supports Many Accounts**: Up to ~20 accounts on original Pebble, up to 100 accounts on newer models
- **Multiple Hash Algorithms**: SHA1, SHA256, and SHA512 support
- **PIN Code Protection**: Optional 3-digit PIN code (000-999) to prevent unauthorized access
- **Standalone Operation**: No Internet access required at all, neither for the settings page nor during usage
- **Clean Design**: Simple, intuitive interface focused on readability
- **Visual Time Indicator**: Progress bar shows remaining time for current code
- **Drag to Reorder**: Easily organize your accounts


## Usage

### Adding Accounts

#### Method 1: QR Code (Easiest)
1. Open TOTPer settings in the Pebble app
2. Click the "QR Code" tab
3. Switch to your camera (or other QR code scanner application) and scan the QR code
4. Copy the scanned data as a text string (it should start with `otpauth://`)
5. Paste the copied code into the text field (you can paste multiple codes at once)
6. Click "Parse & Add Entries"
7. Repeat for additional codes if needed
8. Click "Send to Watch" to sync with your watch

#### Method 2: Manual Entry
1. Open TOTPer settings in the Pebble app
2. Click the "Manual Entry" tab
3. Enter the following details:
   - **Label**: Service name (e.g., "Google", "GitHub")
   - **Account Name**: Your username/email (optional)
   - **Secret**: The base32-encoded secret key
4. Click "Add Entry"
5. Click "Send to Watch" to sync with your watch

### Viewing Codes

1. Open TOTPer on your Pebble
2. Use UP/DOWN buttons to scroll through accounts
3. Press SELECT to open the settings window

### Settings

- **Set PIN / Disable PIN**: 
  - If no PIN is set: Enter a new PIN twice to confirm
  - If PIN is set: Enter the current PIN to disable it
- **Status Bar**: Toggle the status bar with clock display
- **System Information**: View system information (version, memory usage)

### PIN Protection

When PIN is enabled:
- You'll need to enter your PIN when opening the app
- PIN is a 3-digit code (000-999)
- PIN is stored securely on the watch


## FAQ

**Q: Can I use the same secret key in multiple authenticator apps?**  
A: **Yes!** This is actually recommended. When setting up 2FA, scan the QR code with both your phone app (Google Authenticator, Authy, etc.) and TOTPer. Both will generate identical codes. This way you have a backup if you lose your watch.

**Q: Do I need my phone to view codes?**  
A: No! After initial setup, TOTPer works completely standalone. All codes are generated on your watch.

**Q: What if my watch time is wrong?**  
A: TOTP requires accurate time. Make sure your watch is synced with your phone.

**Q: How secure is this?**  
A: TOTPer provides basic security that is suitable for everyday use. Secrets are stored unencrypted in watch storage (like most Pebble apps). The optional PIN uses a simple hash and can be brute-forced if someone has physical access to your watch. It's designed to prevent casual unauthorized access, not to protect against determined attackers with physical access.

**Q: Which services are compatible?**  
A: Any service supporting TOTP (most 2FA systems): Google, GitHub, Microsoft, Facebook, AWS, etc.

**Q: Does this work with HOTP?**  
A: No, only TOTP (Time-based OTP) is supported, not HOTP (counter-based HMAC OTP).

**Q: How many accounts can I store?**  
A: The limit depends on your watch model due to RAM constraints:
- **Original Pebble (Aplite)**: ~20 accounts (24KB RAM)
- **Newer models (Basalt, Chalk, Diorite, Emery)**: Up to 100 accounts

If you experience "Out of memory" errors, try reducing the number of accounts. The app will display a warning if memory is insufficient.

## Troubleshooting

### Codes don't match
- **Most common cause**: Ensure the watch time is accurate (sync with your phone or set manually)
- Verify the secret key was entered correctly (case-insensitive, only A-Z and 2-7)
- Check that digits/period settings match the service requirements

### App crashes
- Try reinstalling the app
- Check available storage on watch (persistent storage)
- Clear some accounts if you have many
- Report issue with debug logs

### Can't add accounts
- Check the available persistent storage on your watch (Settings → System → Storage)
- Verify that the secret is valid base32 (only A-Z and 2-7, no 0, 1, 8, or 9)
- Ensure you're using `otpauth://totp/` URLs (not `hotp`)
- Try manual entry if QR code parsing fails
- If storage is full, remove some unused accounts


## License

This project is open source. See LICENSE file for details.


## Download
* You can always find the latest release at: https://github.com/ClusterM/pebble-topter/releases
* Appstore download will be available soon


## Support the Developer and the Project

* [GitHub Sponsors](https://github.com/sponsors/ClusterM)
* [Buy Me A Coffee](https://www.buymeacoffee.com/cluster)
* [Sber](https://messenger.online.sberbank.ru/sl/Lnb2OLE4JsyiEhQgC)
* [Donation Alerts](https://www.donationalerts.com/r/clustermeerkat)
* [Boosty](https://boosty.to/cluster)
