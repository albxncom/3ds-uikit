# Input, motion and text

The parts of the kit that decide what a touch or a key does, how lists move, how text is broken, and how things move by the clock. All of them are plain C that the kit's host tests run (`host/test_kit.c`); none of them draws. The screens call them and keep their own data.

## Touch

[`touch.h`](../include/touch.h): what a touch presses. A touch on something in or over the scrolling list (a row, the corner button) presses it only while the stylus rests: it shows held after `TOUCH_HOLD_MS`, and once the stylus has moved more than `DRAG_START` px it is a scroll and never presses again. A touch while the list still moves stops it and presses nothing. Anything else shows held at once and acts when the stylus is lifted on it; sliding off cancels, sliding back on presses again.

- `void touch_down(touch_state *t, int target, int in_list, int blocked, int x, int y, uint64_t now);`: a touch begins on `target` (-1: nothing); `blocked`: it presses nothing.
- `void touch_move(touch_state *t, int x, int y, int over);`: the stylus is at `x, y`; `over`: still on the target.
- `int touch_up(touch_state *t, int over);`: the stylus is lifted; returns the target to act on, or -1.
- `int touch_held(const touch_state *t, uint64_t now);`: the target that shows held now, or -1.

In 3DStockTheme the screens, the popup and the cancel tab of a running job take their touches through these four.

## Where buttons take touches

`hits.c` holds where the buttons that `ui.c` draws take touches. The functions are declared in [`ui.h`](../include/ui.h) beside their elements ([elements.md](elements.md)): `ui_hit`, `ui_bar_rect`, `ui_bar_wide_rect`, `ui_tab_hit`, `ui_corner_rect`, `ui_help_rect`, `ui_popup_button`, `ui_settings_rect`. A tab takes a touch on its drawn shape, not in the rounded corner beside it.

## Scrolling

[`scroll.h`](../include/scroll.h): every list's scrolling, one behaviour for all lists. The rows scroll in an area from the screen's top edge down to the bar, under the corner button that floats over them. At scroll 0 the first row starts `inset` px down; a row the cursor moves to is scrolled to show whole between the inset and the area's lower edge.

The area (`scroll_area`: its height, the inset, the rows' height):

- `float scroll_max(const scroll_area *a);`: the furthest the rows scroll; 0 when everything shows.
- `float scroll_clamp(const scroll_area *a, float v);`
- `int scroll_shows(const scroll_area *a, float s, float top, float h);`: 1 when a row shows whole below the inset.
- `float scroll_to_show(const scroll_area *a, float now, float top, float h, float margin);`: the nearest scroll at which a row shows whole.

The selection: nothing is selected when a list opens and after any touch on it. With nothing selected, a move or A selects the first row shown whole and does nothing else; with a row selected, a move goes `delta` rows and A opens it.

- `int select_next(int *selected, long *cursor, int event, long delta, long first, long count);`: `event` is `SELECT_TOUCH`, `SELECT_MOVE` or `SELECT_A`; returns 1 when the row under the cursor is to be opened.

The list (`scroll_shape`: how many rows and how high, where they scroll; `scroll_view`: the cursor, the scroll, the fling, a drag). A move makes the list glide to show the cursor's row, `GLIDE_PCT` of the way per frame. A touch on the rows grabs them: once it has moved `DRAG_START` px they follow the stylus, and when it is lifted they go on at the drag's speed, keeping `FLING_PCT` per frame. Both stop at the ends.

- Rows: `float scroll_top(const scroll_shape *s, size_t i);`, `float scroll_row_h(const scroll_shape *s, size_t i);`, `float scroll_y(const scroll_shape *s, const scroll_view *v, size_t i);` (the row's top on the screen).
- `scroll_area scroll_of(const scroll_shape *s);`: the area the rows scroll in.
- `int scroll_whole(const scroll_shape *s, const scroll_view *v, size_t i);` and `long scroll_first(const scroll_shape *s, const scroll_view *v);`: whether a row shows whole; the first that does.
- `float scroll_target(const scroll_shape *s, const scroll_view *v);`: where the cursor's row shows whole.
- `int scroll_move(const scroll_shape *s, scroll_view *v, long delta);`: a move by `delta` rows; 1 when the selection showed or moved.
- `void scroll_frame(const scroll_shape *s, scroll_view *v);`: once per frame.
- `int scroll_moving(const scroll_view *v);`: it still moves, so a touch only stops it.
- `void scroll_grab(const scroll_shape *s, scroll_view *v, int y, int may_drag);`, `void scroll_drag(const scroll_shape *s, scroll_view *v, int y);`, `int scroll_release(scroll_view *v);`: the stylus comes down, moves, is lifted (1 when it dragged).
- The scroll bar: `void scroll_thumb_to(const scroll_shape *s, scroll_view *v, float y, float track_y, float track_h);` and `void scroll_thumb(const scroll_shape *s, const scroll_view *v, float *at, float *size);` (for `ui_scrollbar`).

3DStockTheme uses it for the folder view, the installed designs, the Badges part's folder and its installed sets.

## The popup's guard and keys

[`popup.h`](../include/popup.h): a popup opens where a button was just pressed and closes where its own button was, so a double tap could press what lies under the stylus next. A touch guard stands at both changes: the popup takes a touch only when the stylus came down `POPUP_GUARD_MS` or more after its first frame, and the screen under it, once it is closed, only that long after the close. A touch that was down when the popup opened or closed presses nothing.

- `void guard_arm(popup_guard *g, int touching);`: a popup opens or closes.
- `void guard_from(popup_guard *g, uint64_t now);`: the guard time runs from now.
- `void guard_lift(popup_guard *g);`: the stylus is lifted.
- `int guard_takes(const popup_guard *g, uint64_t down_at);`: a touch that came down then may press.

Keys: a question (two buttons) has its cursor on the left one, hidden until a key shows it; the first press only shows it. B always cancels; a popup with one button closes on A or B.

- `void focus_open(popup_focus *f);`
- `int popup_key(popup_focus *f, int two, int key);`: `key` is `POPUP_KEY_LEFT`, `POPUP_KEY_RIGHT`, `POPUP_KEY_A` or `POPUP_KEY_B`; returns -1 (nothing to press), 0 the left button, 1 the right or only one.

## The carousel

[`swarm.h`](../include/swarm.h): the list's carousel, header only. The selected item's pane stands in the middle and up to `SWARM_SIDE` on each side, each one further out smaller, lower, tucked further behind and fainter. The selection's position glides from one item to the next in `SWARM_MOVE_MS`, and item `i` is drawn at the distance `i` minus that position. Nothing wraps round. The drawing asks where each pane stands and which icon slot it draws from; 3DStockTheme draws it in its own widget `ui_swarm`.

- `static inline void swarm_slot_x(int k, float apart, float *dx, float *w)`: slot `k`'s width and its distance from the middle.
- `static inline swarm_pane swarm_at(float d, float apart)`: the pane at distance `d` (negative: left).
- `static inline void swarm_span(float pos, size_t count, long *first, long *last)`: the items drawn round a position.
- `static inline float swarm_follow(swarm *s, const void *key, size_t item, uint64_t now, uint32_t frame)`: the position on its way; at once when the carousel starts afresh or jumps by more than `SWARM_SIDE`.
- `static inline float swarm_light(const swarm *s, uint64_t now, int d)`: the idle light on a pane, 0 to 1, or -1.
- The icon texture's slots (`ICON_SLOTS`, the first `SWARM_KEYED` for the carousel, so a pane keeps its slot while it glides): `static inline int icon_slot_keyed(size_t index)`, `static inline void icon_slots_frame(icon_slots *s)`, `static inline void icon_slots_top(icon_slots *s, int right_eye)`, `static inline int icon_slot_next(icon_slots *s)`.

## Text wrapping

[`wrap.h`](../include/wrap.h): text broken into lines and cut to a width. Widths come from a callback (`wrap_width`; on the console the system font through citro2d). Characters are stepped through as citro2d draws them, so what is measured is what is drawn whatever the bytes. No line grows past its buffer, and every cut is marked with "...".

- `size_t wrap_unit(const char *s);`, `size_t wrap_next(const char *s, size_t i);`, `size_t wrap_prev(const char *s, size_t i);`: one character's bytes, the next and the previous character.
- `void wrap_cut(char *out, size_t size, const char *s, float w, int px, int from_end, wrap_width width, void *ctx);`: one line cut at its end or its beginning.
- `int wrap_text(ui_block *b, const char *s, float w, int px, int min_px, int max_lines, float max_h, float lh, wrap_width width, void *ctx);`: a block, made smaller down to `min_px` until it fits (`ui_wrap` draws with it).
- `int wrap_paragraphs(ui_block *b, int count, const char *const s[], float w, int px, int min_px, float max_h, float lh, float gap, wrap_width width, void *ctx);`: paragraphs that fit a height together (the popup's text).
- `int wrap_with_path(ui_block *b, int count, const char *first, const char *path, const char *const rest[], float w, int px, int min_px, float max_h, float lh, float gap, wrap_width width, void *ctx);`: the same with a path in the first paragraph, which is cut at its beginning before any paragraph is cut.

## Curves and the clock

[`ease.h`](../include/ease.h): one clock and one family of curves, so the application moves alike everywhere. Everything goes by the clock in ms, never by counting frames: a late frame skips a part of the way and makes nothing longer.

- `static inline float ease_span(uint64_t now, uint64_t since, unsigned ms)`: how far a movement is, 0 to 1, evenly.
- `static inline float ease_out(float t)`: coming in, fast at first and no speed at the end (a screen's content).
- `static inline float ease_both(float t)`: changing in place, no speed at either end (a popup growing).
- A value that moves to where it is sent within a time and may be sent on before it has arrived (`eased`): `static inline void eased_rest(eased *e, float at)`, `static inline float eased_at(const eased *e, uint64_t now)`, `static inline void eased_to(eased *e, float to, uint64_t now, unsigned ms)`, `static inline int eased_moving(const eased *e, uint64_t now)`.

3DStockTheme moves the installing screen's step marks, the loader and the carousel with them.

[`drift.h`](../include/drift.h): the pattern's drift behind the screens, by the clock.

- `static inline float turn_drift(uint64_t ms)`: how far the pattern has moved along an axis, from 0 up to `PATTERN_TILE`, `PATTERN_DRIFT` px a second.
