#ifndef IP30LEDS_H
#define IP30LEDS_H

/*
 * The LEDs driver reads in a stream of opcodes, two bytes each. The highest
 * two bits of first byte define the opcode type. Next six bits are parameter
 * one, and the last eight bits are parameter two.
 * If a LEDS_LOOP(0) opcode is encountered, the stream is terminated on this
 * opcode and no operations are performed until a new stream is loaded.
 * If a LEDS_LOOP(n>0) opcode is encountered, the whole stream is looped.
 * If neither of these opcodes appears until the end of the stream, the behavior
 * is the same as at LEDS_LOOP(0), however a warning will be printed.
 */

#define LEDS_OP_SET	0
	/* set LED brightness; low bits select LED, next byte sets brightness */
#define LEDS_OP_WAIT	1
	/* wait for n ms, where n=param2 * (1 << param1); if n = 0 then stop */
#define LEDS_OP_LOOP	2
	/* restart the LEDs, waiting for n ms; if n = 0 then stop */
#define LEDS_OP_RSVD	3
	/* reserved opcode */

/*
 * Anyone who wonders why you can't loop without a delay should consider the
 * fact that we are processing this opcode inside the kernel.
 */

#endif
