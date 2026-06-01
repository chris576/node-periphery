'use strict';

// E2E tests: run against the real compiled arm64 native binary.
// These tests execute inside a QEMU arm64 container where /dev/gpiochipN
// and /dev/spidevN.N do not exist. They verify that:
//   1. The arm64 prebuild loads without error.
//   2. The public API surface is intact.
//   3. Hardware operations fail with a hardware error, not a JS crash.

const addon = require('../../index');

describe('native addon loading', () => {
  test('module loads without throwing', () => {
    expect(addon).toBeDefined();
  });

  test('Gpio class is exported', () => {
    expect(typeof addon.Gpio).toBe('function');
  });

  test('spi module is exported', () => {
    expect(addon.spi).toBeDefined();
    expect(typeof addon.spi).toBe('object');
  });
});

describe('Gpio static API', () => {
  const { Gpio } = addon;

  test('Gpio.accessible is false — /dev/gpiochip0 does not exist in QEMU', () => {
    expect(Gpio.accessible).toBe(false);
  });

  test('Gpio.HIGH equals 1', () => {
    expect(Gpio.HIGH).toBe(1);
  });

  test('Gpio.LOW equals 0', () => {
    expect(Gpio.LOW).toBe(0);
  });
});

describe('Gpio hardware error handling', () => {
  const { Gpio } = addon;

  test('constructor throws when /dev/gpiochip0 is absent', () => {
    expect(() => new Gpio(0, 'out')).toThrow();
  });

  test('thrown error is an Error instance', () => {
    let caught;
    try { new Gpio(0, 'out'); } catch (e) { caught = e; }
    expect(caught).toBeInstanceOf(Error);
  });

  test('thrown error message is a non-empty string', () => {
    let caught;
    try { new Gpio(0, 'out'); } catch (e) { caught = e; }
    expect(typeof caught.message).toBe('string');
    expect(caught.message.length).toBeGreaterThan(0);
  });
});

describe('spi static API', () => {
  const { spi } = addon;

  test('MODE0 equals 0', () => { expect(spi.MODE0).toBe(0); });
  test('MODE1 equals 1', () => { expect(spi.MODE1).toBe(1); });
  test('MODE2 equals 2', () => { expect(spi.MODE2).toBe(2); });
  test('MODE3 equals 3', () => { expect(spi.MODE3).toBe(3); });

  test('open is a function', () => {
    expect(typeof spi.open).toBe('function');
  });

  test('openSync is a function', () => {
    expect(typeof spi.openSync).toBe('function');
  });
});

describe('spi hardware error handling', () => {
  const { spi } = addon;

  test('openSync throws when /dev/spidev0.0 is absent', () => {
    expect(() => spi.openSync(0, 0)).toThrow();
  });

  test('open passes an error to the callback when /dev/spidev0.0 is absent', (done) => {
    spi.open(0, 0, (err) => {
      expect(err).toBeInstanceOf(Error);
      done();
    });
  });
});
