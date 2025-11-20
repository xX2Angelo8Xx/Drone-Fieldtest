# LCD Shutdown Display Reference Card

**Quick Visual Guide** - What you'll see on the LCD during shutdown

---

## 📺 Normal Operation
```
┌────────────────┐
│Web Controller  │  ← Line 1: System name
│10.42.0.1:8080  │  ← Line 2: Web address
└────────────────┘
```

---

## 🔴 User Shutdown (GUI Button)

### What You'll See:
```
User clicks "Shutdown System" button in GUI
                ↓
┌────────────────┐
│User Shutdown   │  ← Indicates: Intentional shutdown
│Please Wait...  │  ← System is powering off
└────────────────┘
         (1 second display)
                ↓
         System powers off
                ↓
┌────────────────┐
│User Shutdown   │  ← STAYS VISIBLE!
│Please Wait...  │  ← LCD powered by external 5V
└────────────────┘
```

**Meaning:** You pressed the button. This was intentional. ✅

---

## ⚡ Battery Emergency Shutdown

### Phase 1: Warning (0-4.5 seconds)
```
Battery drops below 14.4V
                ↓
┌────────────────┐
│CRITICAL BATT   │  ← Critical battery detected
│1/10            │  ← Counter: 1st confirmation
└────────────────┘
     (500ms later)
                ↓
┌────────────────┐
│CRITICAL BATT   │
│2/10            │  ← Counter: 2nd confirmation
└────────────────┘
     (continues every 500ms)
                ↓
┌────────────────┐
│CRITICAL BATT   │
│9/10            │  ← Counter: 9th confirmation
└────────────────┘
```

### Phase 2: Shutdown Initiated (at 5 seconds)
```
Counter reaches 10/10
                ↓
┌────────────────┐
│CRITICAL BATT   │
│Stopping Rec... │  ← Recording being stopped (if active)
└────────────────┘
         (500ms - during stopRecording())
                ↓
┌────────────────┐
│Battery Shutdown│  ← Indicates: Emergency shutdown
│Critical!       │  ← Reason: Voltage too low
└────────────────┘
         (2 seconds - user can read message)
                ↓
         System powers off
                ↓
┌────────────────┐
│Battery Shutdown│  ← STAYS VISIBLE!
│Critical!       │  ← LCD powered by external 5V
└────────────────┘
```

**Meaning:** Battery voltage dropped below 14.4V. Emergency shutdown. ⚠️

---

## 🔋 Battery States

### Critical! (14.4V - 14.0V)
```
┌────────────────┐
│Battery Shutdown│
│Critical!       │  ← Voltage between 14.0V and 14.4V
└────────────────┘
```
**Meaning:** Battery reached critical threshold but not completely empty.

### Empty! (< 14.0V)
```
┌────────────────┐
│Battery Shutdown│
│Empty!          │  ← Voltage below 14.0V
└────────────────┘
```
**Meaning:** Battery completely drained. Recharge immediately!

---

## 🔄 Battery Recovery Example

### Scenario: Voltage fluctuation
```
Voltage drops to 14.3V (critical)
                ↓
┌────────────────┐
│CRITICAL BATT   │
│1/10            │  ← Started counting
└────────────────┘
                ↓
┌────────────────┐
│CRITICAL BATT   │
│5/10            │  ← 5th confirmation
└────────────────┘
                ↓
Voltage recovers to 15.0V (above threshold)
                ↓
┌────────────────┐
│Web Controller  │  ← Counter reset! Back to normal
│10.42.0.1:8080  │
└────────────────┘
```

**Console Log:**
```
"✓ Battery recovered to 15.0V - reset critical counter (was: 5)"
```

---

## 🎯 Post-Shutdown Diagnostics

### After powering on Jetson, check LCD:

| LCD Display | Shutdown Type | Action Needed |
|-------------|---------------|---------------|
| `User Shutdown` | Intentional | None - resume operation |
| `Battery Shutdown` / `Critical!` | Emergency | Check battery, review logs |
| `Battery Shutdown` / `Empty!` | Emergency | Recharge battery immediately |
| `Web Controller` | Normal boot | System running normally |
| Blank | Power loss | Check power supply/cables |

---

## ⏱️ Timing Reference

### User Shutdown Timeline:
```
t=0s:   GUI button pressed
        LCD: "User Shutdown" / "Please Wait..."
t=+1s:  Shutdown flags set
t=+2s:  Recording stop (if active)
t=+3s:  WiFi AP teardown
t=+5s:  System powers off
        LCD: STILL shows "User Shutdown" ✅
```

### Battery Shutdown Timeline:
```
t=0s:     Battery drops below 14.4V
          LCD: "CRITICAL BATT" / "1/10"
t=+0.5s:  LCD: "CRITICAL BATT" / "2/10"
t=+1.0s:  LCD: "CRITICAL BATT" / "3/10"
...
t=+4.5s:  LCD: "CRITICAL BATT" / "9/10"
t=+5.0s:  Counter = 10/10 → Shutdown triggered
          LCD: "CRITICAL BATT" / "Stopping Rec..." (if recording)
t=+5.5s:  Recording stopped
          LCD: "Battery Shutdown" / "Critical!"
t=+7.5s:  LCD message displayed (2 seconds)
          Shutdown flags set
t=+10s:   System powers off
          LCD: STILL shows "Battery Shutdown" ✅
```

---

## 🛠️ Troubleshooting

### Problem: LCD blank after shutdown
**Possible Causes:**
1. External 5V power to LCD disconnected
2. LCD I2C communication failed during shutdown
3. System crashed (didn't execute shutdown sequence)

**Solution:**
- Check 5V power connection to LCD
- Review logs: `sudo journalctl -u drone-recorder | tail -50`
- If no shutdown message in logs → crash (not clean shutdown)

### Problem: LCD shows old message from previous shutdown
**Cause:** LCD still powered, showing last message

**Solution:**
- This is NORMAL! LCD retains last message when Jetson is off
- Power cycle LCD (disconnect/reconnect 5V) to clear
- Or just ignore - next boot will update display

### Problem: "CRITICAL BATT" counter stuck at 5/10
**Cause:** Battery voltage fluctuating around 14.4V threshold

**Solution:**
- Normal behavior! Counter increments when V < 14.4V
- Counter resets when V >= 14.4V
- If voltage stabilizes below 14.4V, counter will reach 10/10

---

## 📸 Photos (for field reference)

### User Shutdown:
```
   ┌──────────────────┐
   │  User Shutdown   │
   │  Please Wait...  │
   └──────────────────┘
    [LCD stays lit]
    [Jetson powered off]
```

### Battery Shutdown:
```
   ┌──────────────────┐
   │Battery Shutdown  │
   │   Critical!      │
   └──────────────────┘
    [LCD stays lit]
    [Jetson powered off]
```

**Print this card and keep with drone for field diagnostics!** 📋

---

**Version:** v1.5.4  
**Last Updated:** 19. November 2025
