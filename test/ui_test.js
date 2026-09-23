const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('include/HTML.h', 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];

function context(fetch) {
    const alerts = [];
    const fields = {
        'hour-target': { value: '7872,33' },
        'minute-target': { value: '18874,50' },
        'save-offsets-btn': { disabled: false, classList: { add() {}, remove() {} } },
        'hour-offsets-list': { innerHTML: '', appendChild() {} },
        'minute-offsets-list': { innerHTML: '', appendChild() {} },
    };
    const scope = vm.createContext({
        FormData, Date, window: {}, console: { log() {} },
        document: {
            addEventListener() {}, getElementById(id) { return fields[id]; },
            createElement() { return {}; }, querySelectorAll() { return []; },
        },
        alert(message) { alerts.push(message); },
        setTimeout(callback) { setImmediate(callback); }, fetch,
    });
    vm.runInContext(source, scope);
    return { scope, alerts, fields };
}
const response = (status, data) => ({
    ok: status >= 200 && status < 300, status,
    json: async () => data, text: async () => typeof data === 'string' ? data : JSON.stringify(data),
});

(async () => {
    for (const scenario of ['success', 'rejected', 'write-error', 'network-error', 'edit-while-saving']) {
        let polls = 0;
        const { scope, alerts, fields } = context(async (url, options) => {
            if (url === '/set-offsets') {
                assert.equal(options.body.get('hour_target'), '7872.33');
                assert.equal(options.body.get('minute_target'), '18874.50');
                if (scenario === 'rejected') return response(400, 'Invalid steps');
                if (scenario === 'network-error') throw new Error('Network unavailable');
                return response(202, { id: 42 });
            }
            assert.equal(url, '/command-status?id=42');
            ++polls;
            assert.equal(vm.runInContext('offsetsChanged', scope), true);
            assert.equal(fields['save-offsets-btn'].disabled, true);
            if (polls === 1) {
                if (scenario === 'edit-while-saving') vm.runInContext('markUnsaved()', scope);
                return response(200, { done: false });
            }
            assert.equal(polls, 2);
            return response(200, { done: true, success: scenario !== 'write-error', message: 'NVS result' });
        });
        vm.runInContext('markUnsaved()', scope);
        await vm.runInContext('saveAllOffsets()', scope);
        const failed = ['rejected', 'write-error', 'network-error'].includes(scenario);
        assert.equal(alerts.length, failed ? 1 : 0);
        assert.equal(vm.runInContext('offsetsChanged', scope), failed || scenario === 'edit-while-saving');
        assert.equal(fields['save-offsets-btn'].disabled, false);
        console.log('PASS UI save:', scenario);
    }
    for (const success of [true, false]) {
        let finished = false, refreshed = false, polls = 0;
        const { scope, alerts } = context(async url => {
            if (url === '/live-calibrate') return response(202, { id: 7 });
            if (url === '/command-status?id=7') {
                finished = ++polls === 2;
                return response(200, { done: finished, success, message: 'Calibration result' });
            }
            assert.equal(url, '/get-settings');
            assert.equal(finished, true);
            assert.equal(success, true);
            refreshed = true;
            return response(200, { offsets: { hours: new Array(24).fill(0), minutes: new Array(60).fill(0) } });
        });
        await vm.runInContext("liveCalibrate('minute', -1)", scope);
        assert.equal(refreshed, success);
        assert.equal(alerts.length, success ? 0 : 1);
        assert.equal(vm.runInContext('liveCalibrationPending', scope), false);
        console.log('PASS UI calibration:', success ? 'refresh after execution' : 'execution error');
    }
    // Independent digit values: no implicit -delta stored on the next digit.
    {
        const { scope } = context(() => assert.fail('No network expected'));
        vm.runInContext("applyDigitOffset('minute', 10, 100)", scope);
        assert.equal(vm.runInContext('minuteOffsets[10]', scope), 100);
        assert.equal(vm.runInContext('minuteOffsets[11]', scope), 0);
    }
    for (const sameAxis of [false, true]) {
        const saved = { hours: new Array(24).fill(0), minutes: new Array(60).fill(0) };
        saved.minutes[10] = 40;
        let polls = 0, submitted;
        const { scope, alerts } = context(async (url, options) => {
            if (url === '/live-calibrate') return response(202, { id: 9 });
            if (url === '/set-offsets') {
                submitted = options.body;
                return response(202, { id: 10 });
            }
            if (url === '/command-status?id=10') return response(200, { done: true, success: true });
            if (url === '/command-status?id=9') {
                if (++polls === 1) {
                    // Another edit while waiting for live calibration must also survive.
                    vm.runInContext("applyDigitOffset('minute', 12, 30); markUnsaved()", scope);
                    return response(200, { done: false });
                }
                return response(200, { done: true, success: true });
            }
            assert.equal(url, '/get-settings');
            const fresh = { hours: saved.hours.slice(), minutes: saved.minutes.slice() };
            if (sameAxis) fresh.minutes[10] += 100;
            else fresh.hours[3] = 100;
            return response(200, { offsets: fresh });
        });
        scope.initialOffsets = saved;
        vm.runInContext("mergeSavedOffsets(initialOffsets); applyDigitOffset('minute', 10, 140); markUnsaved()", scope);
        await vm.runInContext(`liveCalibrate('${sameAxis ? 'minute' : 'hour'}', 1)`, scope);
        assert.equal(alerts.length, 0);
        assert.equal(vm.runInContext('minuteOffsets[10]', scope), sameAxis ? 240 : 140);
        assert.equal(vm.runInContext('minuteOffsets[12]', scope), 30);
        assert.equal(vm.runInContext('offsetsChanged', scope), true);
        await vm.runInContext('saveAllOffsets()', scope);
        assert.equal(submitted.get('m10'), sameAxis ? '240' : '140');
        assert.equal(submitted.get('m12'), '30');
        assert.equal(vm.runInContext('offsetsChanged', scope), false);
        assert.equal(vm.runInContext('savedMinuteOffsets[10]', scope), sameAxis ? 240 : 140);
        console.log('PASS UI draft preservation:', sameAxis ? 'same digit' : 'other motor');
    }

})().catch(error => { console.error(error); process.exitCode = 1; });
