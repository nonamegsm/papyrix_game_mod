# Games

Open **Apps > Games** and select **2048**, **Snake**, **Falling Blocks**
(a Tetris-style puzzle), or **Nu, Pogodi!**. Games run offline and do not need
files on the SD card.

Use Up/Down to choose a game and Center to start. On the X4 Pro, tap a game
to start it. Back from the chooser returns to Apps.

| Game | Controls | Goal |
| --- | --- | --- |
| 2048 | Up, Down, Left, Right slide the tiles | Merge equal tiles to reach 2048; you can keep playing afterward |
| Snake | Up, Down, Left, Right steer | Eat the outlined food; avoid the walls and your own body |
| Falling Blocks | Left/Right move, Up rotates, Down lowers one row | Fill horizontal rows to clear them |
| Nu, Pogodi! | Up/Down select the basket height; Left/Right select its side. Touch a chute or one of the four basket pads to select a position directly | Catch eggs from four chutes; one point per catch, three misses end the game |

During a game, Center opens the pause menu. Select **Resume**, **New game**,
or **Choose game**. Back from a game returns to the game chooser. Back from
the pause menu resumes. Hold Power to sleep, as in the other apps.

Snake, Falling Blocks, and Nu, Pogodi! start at a slow pace designed for e-paper. Their pause
menu also offers **Pace: Turn-based**: Snake advances once per direction press,
blocks descend only when you press Down, and Nu, Pogodi! advances once per
basket input (including a repeated tap on the same position). Switch back to
**Pace: Slow** to resume automatic movement. The pace setting is shared by these three games
for the current session. Timers stop while the game is paused.

On the X4 Pro, 2048, Snake, and Falling Blocks show **Left | Down | Right**
above the bottom button bar. **Down** spans the middle half of the screen,
twice the touch width of either side button. Use the top screen margin or
physical Up button for Up (rotation in Falling Blocks). The
bottom bar also provides Games, Menu, Left, and Right controls. In the pause
menu, tap an item to select it.

You can also tap the **top, bottom, left, or right margin** of the game screen
to send Up, Down, Left, or Right. The bottom margin is the strip immediately
above the touch pads. These edge controls are active only during gameplay;
the direction pads, footer, game chooser, and pause menu keep their normal
controls. Top/bottom take priority at corners. In Falling Blocks, the top edge
rotates and the bottom edge lowers the piece, just like the direction buttons.

## Nu, Pogodi!

This is an e-paper adaptation of the four-position egg-catching handheld game,
with pixel drawings by default, or the original LCD graphics through the
[optional local artwork import](../third_party/nu-pogodi/README.md). Imported
graphics retain the SVG outlines and original four wolf/basket positions,
scaled uniformly for portrait and landscape. It uses the
catching idea described in the [original handheld's manual archive](https://game-im02.ru/load/instrukcija_k_igre_ehlektronika_nu_pogodi_arzamas/1-1-0-27),
with a slower clock and simplified three-miss rule.

Move the basket to **Upper left**, **Lower left**, **Upper right**, or
**Lower right** before an egg leaves its chute. The outlined egg at the end of
a chute is still catchable on the next step. The filled basket and the
**Basket:** label show the active position. Eggs arrive one at a time, so two
chutes never require a catch on the same step.

On X4 Pro, tap inside one of the four quarters of the play area, or use the
four named basket pads above the footer. The screen margins and physical
buttons adjust the side or height while preserving the other coordinate.
Top/bottom margins take priority at corners. **Menu** pauses the game and
offers restart and pace controls; **Games** returns to the chooser.

In slow mode, the interval starts at 1.1 seconds and gradually decreases to
0.65 seconds as the score grows, plus panel refresh time. Moving the basket
does not restart the egg timer. Use turn-based mode for play at your own pace.

The display uses fast refreshes between occasional full refreshes to clear
ghosting. Movement includes the display's refresh time, so these games play
more slowly than on an LCD. Scores and boards are kept in memory only;
choosing a game again starts a new round.
