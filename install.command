#!/bin/bash
# BEAT FROG · REZONANZA — installs the plugin system-wide (asks for your password)
cd "$(dirname "$0")"
echo "Installing BEAT FROG..."
sudo mkdir -p /Library/Audio/Plug-Ins/VST3 /Library/Audio/Plug-Ins/Components
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/BEAT FROG.vst3" "/Library/Audio/Plug-Ins/Components/BEAT FROG.component"
sudo cp -R "BEAT FROG.vst3" /Library/Audio/Plug-Ins/VST3/
sudo cp -R "BEAT FROG.component" /Library/Audio/Plug-Ins/Components/
sudo xattr -cr "/Library/Audio/Plug-Ins/VST3/BEAT FROG.vst3" "/Library/Audio/Plug-Ins/Components/BEAT FROG.component"
killall -9 AudioComponentRegistrar 2>/dev/null
echo "Done. Restart Ableton and rescan plug-ins. BEAT FROG is in the REZONANZA folder."
