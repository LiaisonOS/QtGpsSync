# QtGpsSync

A Qt C++ application for syncing system time and running continuous GPS mode using a Bluetooth GPS device (including Android phones running a GPS sharing app).

Part of the [LiaisonOS](https://github.com/LiaisonOS) project.

---

## Features

- **Sync GPS Time** — connects to a paired Bluetooth GPS, reads NMEA RMC sentences, sets system clock with sub-second accuracy, and runs a verification pass to confirm the correction
- **GPS Mode** — binds the Bluetooth device to `/dev/rfcomm0`, creates a `/dev/et-gps` symlink, and starts `gpsd` for continuous GPS availability to other applications
- Live UTC clock display
- Maidenhead grid square computed from GPS fix and saved to user config
- Notifies `et-dashboard` via Unix socket for GPS status indicator

---

## Dependencies

### Runtime

- `rfcomm` — from `bluez` package
- `sdptool` — from `bluez` package
- `gpsd` and `gpspipe` — from `gpsd` package
- `sudo` access for `rfcomm`, `hwclock`, and `settimeofday`

### Build

- Qt 5.x or Qt 6.x with the following modules:
  - `core`, `gui`, `widgets`
  - `serialport` (`libqt5serialport5-dev` or `qt6-serialport-dev`)
- `qmake` or `cmake`
- C++11 or later

Install build dependencies on Debian/Ubuntu:

```bash
sudo apt-get install qt5-qmake qtbase5-dev libqt5serialport5-dev
```

---

## Build

```bash
git clone git@github.com:LiaisonOS/QtGpsSync.git
cd QtGpsSync
qmake QtGpsSync.pro
make -j$(nproc)
```

The binary will be at `./QtGpsSync`.

### Install

```bash
sudo cp QtGpsSync /usr/bin/QtGpsSync
sudo chmod +x /usr/bin/QtGpsSync
```

### Sudoers rule

`QtGpsSync` requires root for `rfcomm bind`, `settimeofday`, and `hwclock`. Add:

```
%sudo ALL=(ALL) NOPASSWD: /usr/bin/rfcomm, /sbin/hwclock, /usr/bin/QtGpsSync
```

---

## Usage

Launch with sudo so the sync operation can set the system clock:

```bash
sudo -E QtGpsSync
```

1. Select your paired Bluetooth GPS device from the dropdown
2. Click **Sync GPS Time** for a one-shot time sync
3. Click **GPS Mode** to start continuous GPS (gpsd on `/dev/et-gps`)

---

## Bluetooth Setup

Pair your GPS device or Android phone (running a GPS sharing app such as *BlueNMEA* or *GPSd Forwarder*) before launching:

```bash
bluetoothctl
  scan on
  pair <MAC>
  trust <MAC>
```

---

## License

GPL v2 — same as the LiaisonOS project.
