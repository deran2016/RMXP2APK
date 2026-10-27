# Experimental Ruby Input bridge

`lunetile_input_register` binds a Java `TouchInputState` to Ruby `Input`. It exports XP button constants and `update`, `press?`, `trigger?`, `repeat?`, `dir4` and `dir8`; `lunetile_input_set_scale` selects the XP timing scale 1 or 3, and unregister releases the JNI global reference. The game thread must already be attached to the Java VM. UI touch/cancellation callbacks update the same synchronized Java state; only `Input.update` snapshots it for Ruby.

The ARM64 Ruby/JNI probe ran on an Android 36.1 x86_64 emulator with ARM64 translation. UI-thread inputs and a separate Ruby thread checked directional/diagonal input, short taps, canceled touches, focus release, invalid button IDs and repeat timing. A separate real-game integration is under development; the diagnostic alone does not prove device multitouch or full game behavior.
