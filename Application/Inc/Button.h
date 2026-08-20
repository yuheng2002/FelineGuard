#ifndef INC_BUTTON_H_
#define INC_BUTTON_H_

/* Turns a button press into a feed request.
 *
 * Polled rather than interrupt-driven.
 * A human press lasts at least milliseconds while the main loop comes around in microseconds,
 * so polling keeps up easily, and it avoids an extra interrupt source.
 *
 * The request is raised on release, not on press:
 * the user can hold the button as long as they like,
 * and one press-and-release counts as exactly one request.
 *
 * No software debounce.
 * The Nucleo's B1 button is filtered in hardware by an RC network on the board (UM1724),
 * so debouncing here would be redundant.
 * If this ever moves to an unfiltered button (e.g. a different board), that changes. */

void Button_Init(void);
void Button_Poll(void);

#endif /* INC_BUTTON_H_ */
