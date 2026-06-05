# iCloud Linux

[![GPL-3.0 License](https://img.shields.io/badge/License-GPL%203.0-blue.svg)](LICENSE)
[![Flatpak](https://img.shields.io/badge/Flatpak-Flathub-green.svg)](https://flatpak.org)

iCloud Linux provides lightweight, native-feeling desktop access to Apple iCloud services on Linux. Each service opens in its own window using GTK4, WebKitGTK 6.0, and Libadwaita.

This project is a modernized fork of [cross-platform/icloud-for-linux](https://github.com/cross-platform/icloud-for-linux), completely rewritten in C++ utilizing GTK4 and Libadwaita to provide native Flatpak packaging.

![iCloud Linux App Store View](https://raw.githubusercontent.com/TaylanTatli/iCloud-Linux/master/flatpak/io.github.TaylanTatli.iCloud-Linux.svg)

---

## Features

- **Individual App Windows:** Launch iCloud Mail, Calendar, Notes, Reminders, Photos, Drive, and more as separate native-feeling windows.
- **Libadwaita Integration:** Modern GNOME 40/50 visual style with rounded window corners and clean headerbars.
- **System Theme Matching:** Automatically adapts to light and dark modes according to your system settings.
- **Secure Storage:** Keeps your sessions active securely using persistent cookies inside the Flatpak sandbox.
- **Lightweight & Fast:** Written in C++ utilizing GTK4 and WebKitGTK 6.0, offering low memory usage and smooth animations.

---

## Available Services

You can launch each service directly from your application menu (launcher) using its unique icon, or via the command line:

| Service Name | Command Line Argument |
| :--- | :--- |
| **iCloud Mail** | `mail` |
| **iCloud Contacts** | `contacts` |
| **iCloud Calendar** | `calendar` |
| **iCloud Photos** | `photos` |
| **iCloud Drive** | `drive` |
| **iCloud Notes** | `notes` |
| **iCloud Reminders** | `reminders` |
| **iCloud Pages** | `pages` |
| **iCloud Numbers** | `numbers` |
| **iCloud Keynote** | `keynote` |
| **iCloud Find My** | `find` |

---

## Installation

### From GitHub Releases (Direct Flatpak Bundle)

1. Download the latest `.flatpak` bundle from the [Releases](https://github.com/TaylanTatli/iCloud-Linux/releases) page.
2. Install it using the terminal:
   ```bash
   flatpak install io.github.TaylanTatli.iCloud-Linux.flatpak
   ```

### Building from Source

To build and package the Flatpak locally:

1. Clone the repository:
   ```bash
   git clone https://github.com/TaylanTatli/iCloud-Linux.git
   cd iCloud-Linux
   ```
2. Build the Flatpak bundle using `flatpak-builder`:
   ```bash
   flatpak-builder --force-clean build flatpak/io.github.TaylanTatli.iCloud-Linux.yml
   ```

---

## Customization

### Changing the Top-Level Domain (TLD)

If you use iCloud in China and need to route through the `.com.cn` domain instead of the default `.com`, you can customize the domain suffix:

1. Create a file named `tld` containing `.com.cn`:
   ```bash
   mkdir -p ~/.var/app/io.github.TaylanTatli.iCloud-Linux/data/icloud-linux/
   echo ".com.cn" > ~/.var/app/io.github.TaylanTatli.iCloud-Linux/data/icloud-linux/tld
   ```
2. Restart the application.

---

## Credits & License

- This project is a fork of [cross-platform/icloud-for-linux](https://github.com/cross-platform/icloud-for-linux).
- Licensed under the **GNU GPL v3.0** License. See the [LICENSE](LICENSE) file for details.
