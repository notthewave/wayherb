# Personal fork of Wayherb

To configure edit the `config.h` in the include directory.

You can pipe things to wayherb by doing the following:

**usage examples:**

send a simple notification
```
wayherb 'a simple notification'
```

show battery percentage in Linux
```
cat /sys/class/power_supply/BAT0/capacity | xargs wayherb
```
