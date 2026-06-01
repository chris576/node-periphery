'use strict';

// Manual Jest mock for node-gyp-build.
// Simulates the native C++ addon so unit tests run without a compiled binary.
// Usage in test files: jest.mock('node-gyp-build');

class MockGpio {
  constructor(offset, direction, ...args) {
    this._offset = offset;
    this._direction = direction;
    this._edge = 'none';
    this._activeLow = false;

    // Parse optional (edge, options) or (options) overloads
    if (args.length > 0) {
      if (typeof args[0] === 'string') {
        this._edge = args[0];
        if (args[1] && typeof args[1] === 'object') {
          this._activeLow = args[1].activeLow || false;
        }
      } else if (args[0] && typeof args[0] === 'object') {
        this._activeLow = args[0].activeLow || false;
      }
    }
  }

  readSync() { return 0; }
  writeSync(_value) {}
  read() { return Promise.resolve(0); }
  write(_value) { return Promise.resolve(); }
  watch(_callback) { return this; }
  unwatch(_callback) { return this; }
  unwatchAll() {}
  direction() { return this._direction; }
  setDirection(direction) { this._direction = direction; }
  edge() { return this._edge; }
  setEdge(edge) { this._edge = edge; }
  activeLow() { return this._activeLow; }
  setActiveLow(invert) { this._activeLow = invert; }
  unexport() {}
}

MockGpio.accessible = false;
MockGpio.HIGH = 1;
MockGpio.LOW = 0;

const mockSpi = {
  open: jest.fn((_bus, _device, ...args) => {
    const cb = args[args.length - 1];
    if (typeof cb === 'function') cb(null, {});
  }),
  openSync: jest.fn(() => ({})),
  MODE0: 0,
  MODE1: 1,
  MODE2: 2,
  MODE3: 3,
};

// node-gyp-build exports a factory: require('node-gyp-build')(__dirname) → addon
module.exports = () => ({ Gpio: MockGpio, spi: mockSpi });
