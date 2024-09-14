# AyarabuPlayer
某寝室再生用。

## Runtime requirement
- Windows OS later than Windows 10
- MSVC 2015-2022 (x64)

## Setup before playing
Prepare all the necessary files, commented below, with proper directories. 

<pre>
assetbundledatas
  ├ ...
  ├ mock
  │  ├ master_data
  │  │  ├ ...
  │  │  ├ sound
  │  │  │  ├ ...
  │  │  │  └ SoundVoiceMasterDatas.any // voice file format table
  │  │  └ ...
  │  └ user_data
  ├ ...
  ├ r18
  │  ├ adventure
  │  │  ├ advstill // still image folder
  │  │  │  ├ ...
  │  │  │  ├ advstill0226
  │  │  │  │  └ ...
  │  │  │  └ ...
  │  │  └ eventdata // script folder
  │  │     ├ ...
  │  │     ├ eventdata0022603.evsc
  │  │     └ ...
  │  ├ movie // video folder
  │  │  └ harem
  │  │     ├ ...
  │  │     ├ chara0226
  │  │     │  └ ...
  │  │     └ ...
  │  └ sound
  │     └ voice // voice folder
  │        └ adv
  │          └ ...
  └ ...
</pre>

## How to play
Select a script file named as `eventdata*03.evsc` from menu `File->Open`.

<pre>
r18/adventure/eventdata
  ├ ...
  ├ eventdata0022603.evsc
  └ ...
</pre>

Then, the scene will be set up according to the specification of the script and format table.

## Mouse function
| Input  | Action  |
| --- | --- |
| Mouse wheel | Scale up/down |
| Left button click | Switch to the next image/video. |
| Left button drag | Move zooming frame. This works only when scaled size is beyond the display resolution. |
| Middle button | Reset scale to default. |
| Right button + mouse wheel | Show the next/previous text. |
| Right button + middle button |Hide/show window's frame and menu. Having hidden, the window goes to the origin of the primary display. |
| Right button + left button | Move window. This works only when the window's frame/menu are hidden. |

## Keyboard function
| Input  | Action  |
| --- | --- |
| C | Switch text colour between black and white. |
| T | Show/hide text. |
| Esc | Close the application. |
| Up | Set up scene with the next script. |
| Down | Set up scene with the previous script. |

## Menu function
| Entry | Item | Function |
| ---- | ---- | ---- 
| File | Open | Open a file select dialogue.
| Audio | Loop | Set/reset audio loop setting.
| - | Setting | Open a dialogue for audio volume and rate setting.
| Video | Pause | Pause video.
| - | Setting | Open a dialogue for video playback rate setting.

## Feature limitation
- Some scenes play audio files not in perfectly ascending order. In this case, the text and played voice would not match.
