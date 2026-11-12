const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = fs.readFileSync('include/HTML.h', 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];

function context(fetch, forms = []) {
    const listeners = {};
    const fields = {
        'hour-target': { value: '7872,33', addEventListener() {}, reportValidity: () => true },
        'minute-target': { value: '18874,50', addEventListener() {}, reportValidity: () => true },
        'save-offsets-btn': { disabled: false },
        'hour-offsets-list': { innerHTML: '', appendChild() {}, contains: () => false },
        'minute-offsets-list': { innerHTML: '', appendChild() {}, contains: () => false },
        'offset-status': { setAttribute(name, value) { this[name] = value; } },
        'live-status': { setAttribute(name, value) { this[name] = value; } },
        'draft-status': {},
        'motion-state': {},
        'time-source': {},
        'check-offset-command': { addEventListener() {} },
        'offset-editor': { addEventListener() {} },
        'retry-offsets': { addEventListener() {} },
        'offsets-container': { disabled: true },
        'wifi-ssid': { dataset: {} },
        'tz-name': { dataset: {} },
        'calibration_hint': { dataset: {} },
        'offset-modal': { open: true, addEventListener() {} },
        'modal-index': { value: '10' },
        'modal-value': { value: '' },
    };
    const scope = vm.createContext({
        FormData, Date, AbortSignal, window: {}, console: { log() {} },
        document: {
            addEventListener(name, callback) { listeners[name] = callback; },
            getElementById(id) { return fields[id]; },
            createElement() { return {}; },
            querySelectorAll(selector) { return selector === '[data-command-form]' ? forms : []; },
        },
        setTimeout(callback) { setImmediate(callback); }, fetch,
    });
    vm.runInContext(source, scope);
    vm.runInContext('offsetsReady = true', scope);
    return { scope, fields, listeners };
}
const response = (status, data) => ({
    ok: status >= 200 && status < 300, status,
    json: async () => data, text: async () => typeof data === 'string' ? data : JSON.stringify(data),
});
const baseSteps = { hour_target_x100: 787233, minute_target_x100: 1887450 };

(async () => {
    {
        let statusReads = 0;
        const { scope, fields } = context(async url => {
            if (url === '/set-offsets') return response(202, { id: 42, boot: 734 });
            assert.equal(url, '/command-status?id=42&boot=734');
            ++statusReads;
            return response(404, 'Command expired');
        });
        vm.runInContext("updateClockState({paused: true, hour_referenced: false, minute_referenced: true, time_source: 'ntp'})", scope);
        assert.equal(fields['motion-state'].textContent, 'Confirm displayed time');
        assert.equal(fields['time-source'].textContent, 'Network time');
        vm.runInContext('markUnsaved()', scope);
        await vm.runInContext('saveAllOffsets()', scope);
        assert.equal(statusReads, 1);
        assert.match(fields['offset-status'].textContent, /expired/);
        assert.equal(fields['save-offsets-btn'].disabled, true);
        assert.equal(fields['check-offset-command'].hidden, true);
        console.log('PASS UI reference status and boot-scoped command results');
    }
    {
        const { scope, fields } = context(async (url, options) => {
            if (url === '/set-offsets') {
                assert.equal(options.body.get('motion_revision'), '17');
                return response(202, { id: 11 });
            }
            return response(200, {
                done: true, success: false, motion_revision: 18, message: 'Motion settings changed.',
            });
        });
        vm.runInContext('motionRevision = 17; markUnsaved()', scope);
        await vm.runInContext('saveAllOffsets()', scope);
        assert.match(fields['offset-status'].textContent, /Motion settings changed/);
        assert.equal(fields['save-offsets-btn'].disabled, true);
        assert.equal(vm.runInContext('offsetsChanged', scope), true);
        console.log('PASS UI stale settings protection');
    }
    {
        let submit;
        const requests = [];
        const form = {
            addEventListener(name, callback) { if (name === 'submit') submit = callback; },
            getAttribute(name) { return name === 'action' ? '/factory-reset' : 'post'; },
        };
        const { scope, fields, listeners } = context(async url => {
            requests.push(url);
            return response(200, {
                base_steps: { hour_target_x100: 787200, minute_target_x100: 1883200 },
                offsets: { hours: Array(24).fill(0), minutes: Array(60).fill(0) },
            });
        }, [form]);
        vm.runInContext('offsetsReady = false', scope);
        listeners.DOMContentLoaded();
        submit.call(form, { defaultPrevented: true, preventDefault() { assert.fail('Canceled'); } });
        assert.equal(fields['offsets-container'].disabled, true);
        await new Promise(resolve => setImmediate(resolve));
        assert.deepEqual(requests, ['/get-settings']);
        assert.equal(fields['offsets-container'].disabled, false);
        assert.equal(fields['minute-target'].value, '18832.00');
        console.log('PASS UI reset cancellation and initial hydration');
    }
    {
        const { scope, fields } = context(async url => {
            if (url === '/live-calibrate') return response(202, { id: 8 });
            if (url === '/command-status?id=8') return response(200, { done: true, success: true });
            return response(500, 'Settings unavailable');
        });
        await vm.runInContext("liveCalibrate('minute', 1)", scope);
        assert.match(fields['offset-status'].textContent, /Calibration applied/);
        assert.equal(fields['save-offsets-btn'].disabled, true);
        assert.equal(fields['retry-offsets'].hidden, false);
        console.log('PASS UI calibration with failed refresh');
    }
    for (const scenario of ['success', 'rejected', 'write-error', 'network-error', 'edit-while-saving']) {
        let polls = 0;
        const { scope, fields } = context(async (url, options) => {
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
        assert.equal(fields['offset-status']['data-state'], failed ? 'error' : 'success');
        assert.equal(vm.runInContext('offsetsChanged', scope), failed || scenario === 'edit-while-saving');
        assert.equal(fields['save-offsets-btn'].disabled, scenario === 'network-error');
        console.log('PASS UI save:', scenario);
    }
    for (const success of [true, false]) {
        let finished = false, refreshed = false, polls = 0;
        const { scope, fields } = context(async url => {
            if (url === '/live-calibrate') return response(202, { id: 7 });
            if (url === '/command-status?id=7') {
                finished = ++polls === 2;
                return response(200, { done: finished, success, message: 'Calibration result' });
            }
            assert.equal(url, '/get-settings');
            assert.equal(finished, true);
            assert.equal(success, true);
            refreshed = true;
            return response(200, { base_steps: baseSteps, offsets: { hours: new Array(24).fill(0), minutes: new Array(60).fill(0) } });
        });
        await vm.runInContext("liveCalibrate('minute', -1)", scope);
        assert.equal(refreshed, success);
        assert.equal(fields['live-status']['data-state'], success ? 'success' : 'error');
        assert.equal(fields['save-offsets-btn'].disabled, false);
        console.log('PASS UI calibration:', success ? 'refresh after execution' : 'execution error');
    }
    {
        let posts = 0, statusReads = 0;
        const { scope, fields } = context(async url => {
            if (url === '/set-offsets') {
                ++posts;
                return response(202, { id: 51 });
            }
            assert.equal(url, '/command-status?id=51');
            if (++statusReads <= 3) throw new Error('Connection interrupted');
            return response(200, { done: true, success: true, motion_revision: 19 });
        });
        vm.runInContext("applyDigitOffset('minute', 10, 100); markUnsaved()", scope);
        await vm.runInContext('saveAllOffsets()', scope);
        assert.equal(fields['save-offsets-btn'].disabled, true);
        assert.equal(fields['check-offset-command'].hidden, false);
        assert.match(fields['offset-status'].textContent, /command 51/);
        vm.runInContext("applyDigitOffset('minute', 10, 120); markUnsaved()", scope);
        await vm.runInContext('checkOffsetCommand()', scope);
        assert.equal(posts, 1);
        assert.equal(fields['save-offsets-btn'].disabled, false);
        assert.equal(fields['check-offset-command'].hidden, true);
        assert.equal(vm.runInContext('savedMinuteOffsets[10]', scope), 100);
        assert.equal(vm.runInContext('minuteOffsets[10]', scope), 120);
        assert.equal(vm.runInContext('offsetsChanged', scope), true);
        console.log('PASS UI resumes status checks without resubmitting offsets');
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
        const { scope, fields } = context(async (url, options) => {
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
            return response(200, { base_steps: baseSteps, offsets: fresh });
        });
        scope.initialOffsets = saved;
        vm.runInContext("mergeSavedOffsets(initialOffsets); applyDigitOffset('minute', 10, 140); markUnsaved()", scope);
        vm.runInContext(`currentEdit = { type: 'minute', index: ${sameAxis ? -1 : 10} }`, scope);
        fields['modal-value'].value = '150';
        await vm.runInContext(`liveCalibrate('${sameAxis ? 'minute' : 'hour'}', 1)`, scope);
        assert.equal(fields['modal-value'].value, sameAxis ? 250 : 150);
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
