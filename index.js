'use strict';

const addon = require('node-gyp-build')(__dirname);

// Symbol.asyncDispose support: await using gpio = new Gpio(...)
if (typeof Symbol.asyncDispose !== 'undefined') {
    addon.Gpio.prototype[Symbol.asyncDispose] = function () {
        this.unexport();
        return Promise.resolve();
    };
}

module.exports = addon;
