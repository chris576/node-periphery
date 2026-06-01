'use strict';

// Explicitly activate the manual mock in __mocks__/node-gyp-build.js
jest.mock('node-gyp-build');

const { spi } = require('../../index');

describe('spi module exports', () => {
  test('spi object is exported', () => {
    expect(spi).toBeDefined();
    expect(typeof spi).toBe('object');
  });

  test('open is a function', () => {
    expect(typeof spi.open).toBe('function');
  });

  test('openSync is a function', () => {
    expect(typeof spi.openSync).toBe('function');
  });

  test('MODE0 equals 0', () => {
    expect(spi.MODE0).toBe(0);
  });

  test('MODE1 equals 1', () => {
    expect(spi.MODE1).toBe(1);
  });

  test('MODE2 equals 2', () => {
    expect(spi.MODE2).toBe(2);
  });

  test('MODE3 equals 3', () => {
    expect(spi.MODE3).toBe(3);
  });
});

describe('spi.open', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('calls the callback with (null, device) on success', (done) => {
    spi.open(0, 0, (err, device) => {
      expect(err).toBeNull();
      expect(device).toBeDefined();
      done();
    });
  });

  test('accepts an options argument before the callback', (done) => {
    spi.open(0, 0, { mode: spi.MODE0 }, (err, device) => {
      expect(err).toBeNull();
      expect(device).toBeDefined();
      done();
    });
  });
});

describe('spi.openSync', () => {
  beforeEach(() => {
    jest.clearAllMocks();
  });

  test('returns a device object', () => {
    const device = spi.openSync(0, 0);
    expect(device).toBeDefined();
  });
});
