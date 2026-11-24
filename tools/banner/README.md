# tools/banner

`make_jingle.py out.wav` synthesises the 2.6 s CIA banner jingle (`meta/audio.wav`): a floppy-drive seek, a rising chip
arpeggio and a chord with a ping-pong echo. Original, no samples; 44.1 kHz stereo 16-bit. Needs python3, numpy and scipy.

```
python3 tools/banner/make_jingle.py meta/audio.wav
```
