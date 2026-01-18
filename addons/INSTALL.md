# Installing Custom chan_mobile.so

This Makefile provides a convenient way to compile, install, and load your custom `chan_mobile.so` module.

## Quick Start

From the `asterisk/addons` directory:

```bash
# Compile, install, and load in one command
make -f Makefile.install

# Or step by step:
make -f Makefile.install compile   # Compile the module
make -f Makefile.install install   # Install to Asterisk module directory
make -f Makefile.install load      # Load in Asterisk
```

## Available Targets

- **`all`** (default) - Compile, install, and load the module
- **`compile`** - Compile `chan_mobile.so` using the Asterisk build system
- **`install`** - Install the module to Asterisk's module directory (with backup)
- **`load`** - Load the module in Asterisk
- **`reload`** - Reload the module in Asterisk
- **`verify`** - Verify the installation (shows file info, module status, etc.)
- **`clean`** - Remove build artifacts
- **`help`** - Show this help message

## Examples

### Full rebuild and install
```bash
make -f Makefile.install clean
make -f Makefile.install all
```

### Just reload after code changes
```bash
make -f Makefile.install reload
```

### Verify installation
```bash
make -f Makefile.install verify
```

## How It Works

1. **Compile**: Uses the existing Asterisk build system (`Makefile`) to compile `chan_mobile.so`
2. **Install**: 
   - Backs up existing module (if present)
   - Copies the new module to Asterisk's module directory
   - Sets proper permissions
3. **Load**: 
   - Unloads old module (if loaded)
   - Loads the new module
   - Shows module status

## Module Directory

The Makefile automatically detects the Asterisk module directory by:
1. Querying running Asterisk: `asterisk -rx "core show settings"`
2. Falling back to common locations:
   - `/usr/lib/asterisk/modules`
   - `/usr/lib64/asterisk/modules`
   - `/usr/local/lib/asterisk/modules`

## Notes

- Requires `sudo` for installation (copying to system directory)
- Creates automatic backups before overwriting existing modules
- The module must be compiled against the same Asterisk version that's running
- Check version compatibility: `asterisk -rx "core show version"`
