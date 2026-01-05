# AyarabuPlayer

某寝室再生用。

## Runtime requirement

- Windows OS later than Windows 10
- MSVC 2015-2022 (x64)

## How to play

Prepare all the necessary files, commented below, with proper directories. 

<pre>
assetbundledatas
  ├ ...
  ├ mock
  │  ├ master_data
  │  │  ├ ...
  │  │  ├ sound
  │  │  │  ├ ...
  │  │  │  └ SoundVoiceMasterDatas.any // Voice file format table
  │  │  └ ...
  │  └ user_data
  ├ ...
  ├ r18
  │  ├ adventure
  │  │  ├ advstill // Still image folder
  │  │  │  ├ ...
  │  │  │  ├ advstill0226
  │  │  │  │  ├ still0226_scene01.png
  │  │  │  │  └ ...
  │  │  │  └ ...
  │  │  └ eventdata // Script folder
  │  │     ├ ...
  │  │     ├ eventdata0022603.evsc // Select a script file like this
  │  │     └ ...
  │  ├ movie // Video folder
  │  │  └ harem
  │  │     ├ ...
  │  │     ├ chara0226
  │  │     │  ├ movie0226_scene01.mp4
  │  │     │  └ ...
  │  │     └ ...
  │  └ sound
  │     └ voice // Voice folder
  │        └ adv
  │          ├ ...
  │          ├ suiten
  │          │  ├ ep3
  │          │  └ ep4
  │          │    ├ VoiceSuiten_S4_0001.m4a
  │          │    └ ...
  │          └ ...
  └ ...
</pre>

Then select a script file named like `eventdata*03.evsc` from menu `File->Open`.

<pre>
r18/adventure/eventdata
  ├ ...
  ├ eventdata0022603.evsc
  └ ...
</pre>

## Menu function

| Entry | Item | Function |
| ---- | ---- | ---- |
| File | Open | Show a dialogue to select a script file to open. |
| - | Next | Open the next script. |
| - | Previous | Open the previous script. |
| Setting | Audio | Show a dialogue for audio player volume/playback rate. |
| - | Video | Show a dialogue for video player playback rate. |
| Image | Pause | Pause video. |
| - | Sync | Turn on/off the synchronisation of still/videos with texts. |

- When the video is being paused, left click steps the video one frame.
- When synchronisation is turned off, left click switches the still/video.

## Mouse function

| Input | Action |
| --- | --- |
| Mouse wheel | Scale up/down |
| Left drag | Move the region to be shown. This works only when scaled beyond the display resolution. |
| Middle click | Reset the scale to the default. |
| Right click | Show context menu to jump to the scene where still/video is switched. |
| Right pressed + mouse wheel | Fast-forward/rewind text. |
| Right pressed + middle click | Hide/show window's border. Having hidden, the window goes to the origin of the primary display. |
| Right pressed + left click | Move borderless window. |

## Keyboard function

| Input | Action |
| --- | --- |
| <kbd>Esc</kbd> | Close the application. |
| <kbd>C</kbd> | Toggle the text colour between black and white. |
| <kbd>T</kbd> | Show/hide the text. |
| <kbd>↑</kbd> | Open the previous script. |
| <kbd>↓</kbd> | Open the next script. |
| <kbd>→</kbd> | Fast-forward the text. |
| <kbd>←</kbd> | Rewind the text. |
