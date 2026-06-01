export type GpioDirection = 'in' | 'out' | 'high' | 'low';
export type GpioEdge = 'none' | 'rising' | 'falling' | 'both';

export interface GpioOptions {
    /** Which /dev/gpiochipN to use. Default: 0 */
    chip?: number;
    /** Invert read/write values. Default: false */
    activeLow?: boolean;
    /** Kernel-native debounce in milliseconds. Default: 0 (disabled) */
    debounceTimeout?: number;
}

export declare class Gpio {
    constructor(offset: number, direction: GpioDirection, options?: GpioOptions);
    constructor(offset: number, direction: GpioDirection, edge: GpioEdge, options?: GpioOptions);

    /** Read GPIO value synchronously. Returns 0 or 1. */
    readSync(): 0 | 1;

    /** Write GPIO value synchronously. */
    writeSync(value: 0 | 1): void;

    /** Read GPIO value asynchronously. Resolves with 0 or 1. */
    read(): Promise<0 | 1>;

    /** Write GPIO value asynchronously. */
    write(value: 0 | 1): Promise<void>;

    /** Register a callback for hardware interrupt events. Returns this for chaining. */
    watch(callback: (err: Error | null, value: 0 | 1) => void): this;

    /** Remove a specific interrupt callback (or all if omitted). Returns this. */
    unwatch(callback?: (err: Error | null, value: 0 | 1) => void): this;

    /** Remove all interrupt callbacks. */
    unwatchAll(): void;

    /** Get current direction. */
    direction(): GpioDirection;

    /** Reconfigure direction without reopening the line fd. */
    setDirection(direction: GpioDirection): void;

    /** Get current interrupt edge setting. */
    edge(): GpioEdge;

    /** Reconfigure interrupt edge without reopening the line fd. */
    setEdge(edge: GpioEdge): void;

    /** Get activeLow setting. */
    activeLow(): boolean;

    /** Set activeLow — inverts all read/write values. */
    setActiveLow(invert: boolean): void;

    /** Release the GPIO line and close file descriptors. */
    unexport(): void;

    /** Async resource disposal — enables `await using gpio = new Gpio(...)` syntax. */
    [Symbol.asyncDispose](): Promise<void>;

    /** true if /dev/gpiochip0 is accessible by the current process. */
    static readonly accessible: boolean;

    static readonly HIGH: 1;
    static readonly LOW: 0;
}

export declare const spi: any;
