'use strict';

// Explicitly activate the manual mock in __mocks__/node-gyp-build.js
jest.mock('node-gyp-build');

const { Gpio } = require('../../index');

describe('Gpio module exports', () => {
  test('Gpio class is exported', () => {
    expect(typeof Gpio).toBe('function');
  });

  test('HIGH constant equals 1', () => {
    expect(Gpio.HIGH).toBe(1);
  });

  test('LOW constant equals 0', () => {
    expect(Gpio.LOW).toBe(0);
  });

  test('accessible is a boolean', () => {
    expect(typeof Gpio.accessible).toBe('boolean');
  });
});

describe('Gpio Symbol.asyncDispose', () => {
  const supportsAsyncDispose = typeof Symbol.asyncDispose !== 'undefined';

  test('asyncDispose is attached to prototype when Symbol.asyncDispose is available', () => {
    if (!supportsAsyncDispose) return;
    expect(typeof Gpio.prototype[Symbol.asyncDispose]).toBe('function');
  });

  test('asyncDispose calls unexport() exactly once', async () => {
    if (!supportsAsyncDispose) return;
    const gpio = new Gpio(1, 'out');
    const spy = jest.spyOn(gpio, 'unexport');
    await gpio[Symbol.asyncDispose]();
    expect(spy).toHaveBeenCalledTimes(1);
  });

  test('asyncDispose returns a resolved Promise', async () => {
    if (!supportsAsyncDispose) return;
    const gpio = new Gpio(1, 'out');
    await expect(gpio[Symbol.asyncDispose]()).resolves.toBeUndefined();
  });
});

describe('Gpio constructor', () => {
  test('creates instance with offset and direction', () => {
    const gpio = new Gpio(4, 'out');
    expect(gpio).toBeInstanceOf(Gpio);
  });

  test('creates instance with offset, direction, and options', () => {
    const gpio = new Gpio(4, 'in', { activeLow: true, debounceTimeout: 10 });
    expect(gpio).toBeInstanceOf(Gpio);
  });

  test('creates instance with offset, direction, edge, and options', () => {
    const gpio = new Gpio(4, 'in', 'rising', { chip: 1, debounceTimeout: 5 });
    expect(gpio).toBeInstanceOf(Gpio);
  });
});

describe('Gpio prototype methods', () => {
  let gpio;

  beforeEach(() => {
    gpio = new Gpio(4, 'out');
  });

  test('readSync() returns 0 or 1', () => {
    const value = gpio.readSync();
    expect(value === 0 || value === 1).toBe(true);
  });

  test('writeSync(0) does not throw', () => {
    expect(() => gpio.writeSync(0)).not.toThrow();
  });

  test('writeSync(1) does not throw', () => {
    expect(() => gpio.writeSync(1)).not.toThrow();
  });

  test('read() returns a Promise', () => {
    expect(gpio.read()).toBeInstanceOf(Promise);
  });

  test('read() resolves with 0 or 1', async () => {
    const value = await gpio.read();
    expect(value === 0 || value === 1).toBe(true);
  });

  test('write() returns a Promise', () => {
    expect(gpio.write(1)).toBeInstanceOf(Promise);
  });

  test('write() resolves without a value', async () => {
    await expect(gpio.write(0)).resolves.toBeUndefined();
  });

  test('watch() returns this for chaining', () => {
    const result = gpio.watch(() => {});
    expect(result).toBe(gpio);
  });

  test('unwatch() returns this for chaining', () => {
    const cb = () => {};
    gpio.watch(cb);
    expect(gpio.unwatch(cb)).toBe(gpio);
  });

  test('unwatchAll() does not throw', () => {
    expect(() => gpio.unwatchAll()).not.toThrow();
  });

  test('direction() returns the configured direction', () => {
    const g = new Gpio(4, 'in');
    expect(g.direction()).toBe('in');
  });

  test('setDirection() updates direction()', () => {
    gpio.setDirection('in');
    expect(gpio.direction()).toBe('in');
  });

  test('edge() returns the configured edge', () => {
    const g = new Gpio(4, 'in', 'falling');
    expect(g.edge()).toBe('falling');
  });

  test('setEdge() updates edge()', () => {
    gpio.setEdge('both');
    expect(gpio.edge()).toBe('both');
  });

  test('activeLow() returns the configured activeLow value', () => {
    const g = new Gpio(4, 'out', { activeLow: true });
    expect(g.activeLow()).toBe(true);
  });

  test('setActiveLow() updates activeLow()', () => {
    gpio.setActiveLow(true);
    expect(gpio.activeLow()).toBe(true);
  });

  test('unexport() does not throw', () => {
    expect(() => gpio.unexport()).not.toThrow();
  });
});
