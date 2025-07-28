# CI: AppImage Packaging for STC_editor

This repository uses GitHub Actions to automatically build and package `STC_editor` as an AppImage on every push to the `master` branch.

---

## 🧰 Why this setup?

AppImage packaging for Qt applications — especially those using `QtWebEngine` — is known to be **non-trivial**. We use a combination of:

- ✅ `linuxdeployqt` — to automate dependency bundling
- ✅ Qt6 from the system (Ubuntu 22.04)
- ✅ Manual bundling of `QtWebEngine` runtime files
- ✅ GCC 13 — for `std::format` and modern C++ support

---

## 💡 Why Ubuntu 22.04?

Although newer Ubuntu versions are available, we use Ubuntu 22.04 because:

- `linuxdeployqt` requires an **older glibc** runtime to ensure compatibility with other Linux systems.
- The official `linuxdeployqt` AppImage itself was built on Ubuntu 16.04, and works best with environments based on Ubuntu 20.04 or 22.04.

> ⚠️ Using a newer host system (e.g. Ubuntu 24.04) may cause the AppImage not to run on older Linux distributions due to glibc incompatibility.

---

## 🧱 Why GCC 13?

Ubuntu 22.04 ships with GCC 11 by default, which lacks support for certain C++20 features such as `std::format`. To enable those, we install GCC 13 from the `ubuntu-toolchain-r/test` PPA and use it explicitly.

---

## 🧩 Why manually bundle QtWebEngine resources?

The default Qt6 packages (`qt6-webengine-dev`, etc.) from Ubuntu **do not include some necessary runtime resources** such as:

- `icudtl.dat`
- `v8_context_snapshot.bin`
- translations

To fix this, we include a folder in the repository called `qt-runtime/`, which contains the necessary files from a Qt installation made with the **official Qt Installer** (`qt-unified-linux-x64...run`).

These are copied into the `build/` directory during the packaging step.

---

## 📦 Is this legal?

Yes.

All runtime Qt libraries and resources are:

- Redistributable under the LGPL, assuming dynamic linking (which we use)
- Unmodified from official Qt distributions
- Extracted at runtime into a mount point (`/tmp/.mount...`), which users can inspect, modify, or repackage if needed

> 🛠️ If needed, users can extract the AppImage using:
>
> ```bash
> ./STC_editor-x86_64.AppImage --appimage-extract
> ```
> Then replace Qt libraries under `squashfs-root/`, and rebuild the AppImage using:
>
> ```bash
> ./appimagetool-x86_64.AppImage squashfs-root
> ```

---

## ✅ Summary

| Component              | Reason                                                                 |
|------------------------|------------------------------------------------------------------------|
| Ubuntu 22.04           | Compatibility with `linuxdeployqt` and older glibc                    |
| GCC 13                 | Support for `std::format` and C++20                                    |
| linuxdeployqt          | Standardized Qt AppImage bundler, but requires careful environment     |
| qt-runtime/            | Includes missing QtWebEngine resources not available in system packages |
| AppImage               | Portable, single-file Linux distribution format                        |

---

Feel free to contribute improvements to this setup or migrate to a more modern alternative (e.g., custom AppDir + `appimagetool`) in the future.
