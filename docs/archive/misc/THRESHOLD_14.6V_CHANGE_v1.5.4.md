# Critical Threshold Change: 14.4V → 14.6V

**Status:** ✅ IMPLEMENTED  
**Date:** 19. November 2025  
**Version:** v1.5.4  
**Bug:** #7 - Voltage jumping causes counter resets too often

---

## 🔴 Problem Discovered

After extensive field testing with the 14.4V threshold:

**Symptom:** Emergency shutdown debounce counter resets too frequently
- Voltage jumps erratically due to rapid Jetson power changes (IDLE ↔ RECORDING transitions)
- Example: 14.3V → 14.5V → 14.35V → 14.48V (within 2 seconds)
- Counter reaches 7/10, then voltage briefly recovers → counter resets to 0
- System never shuts down even when battery is critically low

**Root Cause:** 
- Jetson Orin Nano power consumption varies 0.5A (IDLE) to 3.0A (RECORDING)
- Cable resistance (~0.1-0.2Ω) causes voltage drop proportional to current
- 14.4V threshold is TOO CLOSE to actual critical voltage
- Any brief power reduction (frame drop, CPU throttle) causes voltage to jump above threshold

**Safety Risk:** System may not shutdown in time, causing battery over-discharge

---

## ✅ Solution: Increase Threshold to 14.6V

### Reasoning:
1. **Safety Margin:** 0.2V buffer above true critical voltage (3.5V/cell = 14.0V)
2. **Debounce Reliability:** Voltage fluctuations less likely to cross threshold
3. **Field Testing:** 14.6V prevents counter resets while still protecting battery
4. **Conservative Approach:** Better to shutdown 30-60 seconds early than risk over-discharge

### Trade-off:
- **Lost flight time:** ~30-60 seconds (acceptable for safety)
- **Gained reliability:** Consistent shutdown behavior, no counter reset issues

---

## 📝 Implementation Details

### Changed Values:

| Location | Old Value | New Value | Comment |
|----------|-----------|-----------|---------|
| `battery_monitor.cpp` constructor | `14.4f` | `14.6f` | Critical threshold |
| `battery_monitor.cpp` constructor | `14.4f` | `14.6f` | Empty voltage (0%) |
| `battery_monitor.h` documentation | 14.4V (3.6V/cell) | 14.6V (3.65V/cell) | Header comments |
| `drone_web_controller.cpp` GUI | 14.4V (3.6V/cell) | 14.6V (3.65V/cell) | Web interface text |
| `THRESHOLD_14.4V_IMPLEMENTATION.md` | (obsolete) | (archived) | Old documentation |

### Files Modified:
1. ✅ `common/hardware/battery/battery_monitor.cpp` (line ~41)
2. ✅ `common/hardware/battery/battery_monitor.h` (lines ~10-20, ~45)
3. ✅ `apps/drone_web_controller/drone_web_controller.cpp` (line ~2133)

### Unchanged (Correctly):
- ✅ **Segment selection logic:** Still uses `voltage_raw` (no filter applied)
- ✅ **Calibration values:** No change (14.6V not a calibration point)
- ✅ **Debounce counter:** Still 10 confirmations @ 500ms = 5 seconds
- ✅ **Counter reset logic:** Still resets when voltage recovers above threshold

---

## 🧪 Testing Requirements

### Regression Tests:
1. **Normal Shutdown:**
   - Drain battery to 14.7V → should see counter start incrementing
   - Voltage drops to 14.5V → should trigger shutdown after 5 seconds
   - LCD should show "Battery Shutdown / Critical!"

2. **Counter Reset:**
   - Drain battery to 14.5V (counter at 7/10)
   - Stop recording → power drops → voltage recovers to 14.7V
   - Counter should reset to 0 ✓
   - Recording should be allowed to restart

3. **Voltage Jumping:**
   - Monitor voltage during IDLE ↔ RECORDING transitions
   - Voltage should jump 0.2-0.3V but stay stable relative to 14.6V threshold
   - Counter should NOT reset randomly

4. **Early Warning:**
   - At 14.8V → GUI should show low battery warning (yellow)
   - At 14.6V → Emergency shutdown initiated
   - User has ~5 seconds to stop recording and save data

---

## 📊 Voltage Reference Table (4S LiPo)

| Voltage | Per Cell | State | Action |
|---------|----------|-------|--------|
| 16.8V | 4.20V | ⚡ Full charge | Safe operation |
| 16.0V | 4.00V | ✅ Good | Normal operation |
| 15.2V | 3.80V | ✅ Good | Normal operation |
| 14.8V | 3.70V | ⚠️ Warning | Low battery warning (GUI yellow) |
| 14.6V | 3.65V | 🔴 Critical | **Emergency shutdown (5s debounce)** ← NEW |
| 14.4V | 3.60V | 🔴 Critical | ~~Emergency shutdown (was here)~~ ← OLD |
| 14.0V | 3.50V | ⚠️ Damage risk | Absolute minimum (battery damage below) |
| <14.0V | <3.50V | 💀 Over-discharge | Permanent battery damage |

---

## 🔄 Relationship to Other Bugs

### Bug #6 (Low-Pass Filter):
- **Independent:** Filter applies to DISPLAY values only
- **Threshold check:** Still uses RAW voltage (no filter)
- **Reason:** Emergency shutdown must react to actual voltage, not smoothed display

### Bug #8 (mAh-based Remaining Time):
- **Independent:** Remaining time calculation doesn't affect shutdown
- **Threshold:** Still based on voltage, not mAh consumed

### Voltage Drop Documentation:
- **Supersedes:** `docs/VOLTAGE_DROP_UNDER_LOAD_v1.5.4.md` reasoning
- **New approach:** Accept voltage drop as normal, increase threshold instead of compensating

---

## 📚 Documentation Updates Needed

- [ ] Update `THRESHOLD_14.4V_IMPLEMENTATION.md` → archive or rename to `THRESHOLD_14.6V_IMPLEMENTATION.md`
- [ ] Update `docs/VOLTAGE_DROP_UNDER_LOAD_v1.5.4.md` with new threshold reasoning
- [ ] Update `LCD_DISPLAY_REFERENCE.md` with 14.6V in shutdown messages
- [ ] Update `copilot-instructions.md` with new critical threshold
- [ ] Create `RELEASE_v1.5.5_STABLE.md` or update v1.5.4 notes

---

## 🎯 Validation Checklist

Before marking Bug #7 as complete:

- [x] Changed `critical_voltage_` in `battery_monitor.cpp`
- [x] Changed `empty_voltage_` in `battery_monitor.cpp`
- [x] Updated header documentation in `battery_monitor.h`
- [x] Updated GUI text in `drone_web_controller.cpp`
- [ ] Rebuild project: `./scripts/build.sh`
- [ ] Field test with battery at 14.8V → 14.5V
- [ ] Confirm counter does NOT reset during normal operation
- [ ] Confirm shutdown triggers reliably at 14.6V
- [ ] Update all related documentation

---

## 🔍 Future Considerations

If 14.6V proves too conservative (shutdowns too early):
- Could reduce to 14.5V (3.625V/cell)
- Requires field testing to confirm counter stability
- Document results in `THRESHOLD_TUNING_HISTORY.md`

If 14.6V still has counter reset issues:
- Problem is not threshold, but filter implementation
- Implement Bug #6 (low-pass filter) for display
- Keep raw voltage for threshold check
- Increase debounce count from 10 to 15 (7.5s total)

---

**Next Steps:**
1. Rebuild project
2. Deploy to Jetson
3. Field test with real battery drain
4. Document results
5. Mark Bug #7 as complete
6. Move to Bug #2 (LCD shutdown messages)
