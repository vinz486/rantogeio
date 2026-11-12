const char CONFIG_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Rantoge · Clock settings</title>

    <style>
        :root {
            color-scheme: light;
            --ink: #242235;
            --muted: #5c596e;
            --line: #dedce6;
            --surface: #fff;
            --canvas: #f5f4f8;
            --accent: #4e3d90;
            --accent-hover: #3d2d77;
            --accent-soft: #eeebf8;
            --danger: #a02139;
            --danger-soft: #faedf0;
            --success: #236047;
        }

        * { box-sizing: border-box; }
        [hidden] { display: none !important; }
        html { -webkit-text-size-adjust: 100%; }
        body {
            margin: 0;
            background: var(--canvas);
            color: var(--ink);
            font: 1rem/1.5 "Inter", system-ui, sans-serif;
            accent-color: var(--accent);
        }

        h1, h2, h3, p { margin: 0; }
        h1, h2 { letter-spacing: -.025em; line-height: 1.2; }
        h1 { font-size: 1.375rem; }
        h2 { font-size: 1.625rem; }
        h3 { font-size: 1rem; font-weight: 700; }
        a { color: var(--accent); text-underline-offset: .2em; }
        :focus-visible { outline: 3px solid #765abe; outline-offset: 3px; }
        ::selection { background: #e2d9ff; color: var(--ink); }

        .wrap { width: min(100% - 2.5rem, 72rem); margin-inline: auto; }
        .masthead { background: #28213d; color: #fff; }
        .masthead .wrap {
            min-height: 4.5rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            gap: 1rem;
        }
        .masthead .btn { color: #fff; background: transparent; border-color: #817496; }
        .masthead .btn:hover { background: #3e3255; }

        .layout {
            display: grid;
            grid-template-columns: 10.5rem minmax(0, 1fr);
            gap: 2rem;
            align-items: start;
            padding-block: 1.75rem 3rem;
        }
        .sections { position: sticky; top: 1.5rem; display: grid; gap: .2rem; }
        .sections a {
            display: flex;
            align-items: center;
            min-height: 2.75rem;
            padding: .45rem .75rem;
            border-radius: .5rem;
            color: var(--ink);
            font-size: .875rem;
            font-weight: 650;
            text-decoration: none;
        }
        .sections a:hover { background: var(--accent-soft); color: var(--accent); }

        .section + .section { margin-top: 2.25rem; }
        .section-title { display: flex; align-items: center; flex-wrap: wrap; gap: .5rem 1rem; margin-bottom: .875rem; }
        .panel { background: var(--surface); border: 1px solid var(--line); border-radius: .875rem; }
        .subsection { padding: 1.125rem 1.25rem; min-width: 0; }
        .panel > * + .subsection { border-top: 1px solid var(--line); }
        .two-column { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); }
        .two-column { gap: 1.25rem; }
        .two-column > * + * { border-inline-start: 1px solid var(--line); padding-inline-start: 1.25rem; }
        .lede, .hint { color: var(--muted); max-width: 69ch; }
        .lede { margin-top: .375rem; }
        .hint { font-size: .875rem; }
        .stack { display: grid; gap: .75rem; align-content: start; }
        .actions { display: flex; flex-wrap: wrap; gap: .5rem; align-items: center; }
        .form-fields { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: .75rem; }
        .form-fields.date { grid-template-columns: repeat(3, minmax(0, 1fr)); }

        .motion-toolbar {
            position: sticky;
            top: 0;
            z-index: 1;
            display: flex;
            align-items: center;
            justify-content: space-between;
            flex-wrap: wrap;
            gap: .5rem .75rem;
            padding: .65rem 1.25rem;
            background: #f4f1fb;
            border-bottom: 1px solid #ded6ee;
            border-radius: .875rem .875rem 0 0;
        }
        .motion-toolbar .hint, .motion-toolbar .status { flex-basis: 100%; }
        .motion-toolbar .status:empty { margin-top: -.5rem; }
        .motion-state { font-size: .875rem; font-weight: 650; color: var(--accent); }
        .axis-actions {
            display: grid;
            grid-template-columns: minmax(4rem, 1fr) repeat(2, minmax(0, 1fr));
            align-items: center;
            gap: .5rem;
        }
        .axis-actions .btn { font-variant-numeric: tabular-nums; }
        #motion :is(button, input, select) { scroll-margin-top: 9rem; }

        button, input, select { font: inherit; }
        label, legend { font-size: .875rem; font-weight: 650; }
        label { display: block; }
        fieldset { min-inline-size: 0; border: 0; margin: 0; padding: 0; }
        legend { padding: 0; margin-bottom: .375rem; }
        input:not([type=radio], [type=checkbox]), select {
            display: block;
            width: 100%;
            min-height: 2.75rem;
            margin-top: .25rem;
            padding: .5rem .625rem;
            color: var(--ink);
            background: var(--surface);
            border: 1px solid #918da2;
            border-radius: .5rem;
            font-size: 1rem;
            font-variant-numeric: tabular-nums;
        }
        input::placeholder { color: var(--muted); opacity: 1; }
        input:disabled, select:disabled { background: var(--canvas); }
        .choice { display: flex; align-items: center; gap: .5rem; min-height: 2.75rem; font-weight: 500; }
        .choice input { margin: 0; width: 1.125rem; height: 1.125rem; }
        .btn {
            display: inline-flex;
            align-items: center;
            justify-content: center;
            gap: .375rem;
            min-height: 2.75rem;
            padding: .48rem .85rem;
            border: 1px solid #918da2;
            border-radius: .5rem;
            background: var(--surface);
            color: var(--ink);
            font: 650 .875rem/1.25 "Inter", system-ui, sans-serif;
            text-decoration: none;
            text-align: center;
            cursor: pointer;
        }
        .btn:hover { background: var(--canvas); }
        .btn:active { background: var(--accent-soft); }
        .btn-primary { color: #fff; background: var(--accent); border-color: var(--accent); }
        .btn-primary:hover, .btn-primary:active { background: var(--accent-hover); }
        .btn-stop { color: #fff; background: var(--danger); border-color: var(--danger); }
        .btn-stop:hover, .btn-stop:active { background: #80192c; }
        .btn-danger { color: var(--danger); background: var(--danger-soft); border-color: #bd7b89; }
        .btn-danger:hover { background: #f7dce3; }
        .btn:disabled, fieldset:disabled .btn {
            background: var(--canvas);
            color: var(--muted);
            border-color: var(--line);
            cursor: default;
        }
        .status { font-size: .875rem; color: var(--muted); overflow-wrap: anywhere; }
        .status:empty { margin: 0; }
        .status[data-state="error"] { color: var(--danger); }
        .status[data-state="success"] { color: var(--success); }
        .status[data-state="pending"] { color: var(--accent); }
        .status:not(:empty) { margin-block: .35rem; }
        .stack > .status:empty { margin-top: -.75rem; }
        #check-cycle-command { justify-self: start; }

        .offset-lists { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 1.25rem; }
        .offset-list { list-style: none; padding: 0; margin: 0; }
        .offset-list:empty::after { content: "No digit offsets"; color: var(--muted); font-size: .875rem; }
        .offset-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            gap: .25rem .75rem;
            padding-block: .375rem;
            border-bottom: 1px solid var(--line);
        }
        .offset-item strong, .offset-value { font-variant-numeric: tabular-nums; }
        .offset-value { color: var(--muted); margin-left: .5rem; }
        .offset-item .btn { padding-inline: .6rem; }
        .offset-footer { margin-top: 1rem; padding-top: 1rem; border-top: 1px solid var(--line); }
        .draft-status { color: var(--accent); font-size: .875rem; font-weight: 650; }
        .log summary { cursor: pointer; font-size: .875rem; font-weight: 650; padding-block: .5rem; }
        .log pre {
            max-height: 12rem;
            overflow: auto;
            white-space: pre-wrap;
            overflow-wrap: anywhere;
            background: var(--canvas);
            border-radius: .5rem;
            padding: .75rem;
            margin: .5rem 0 0;
            font-size: .8125rem;
        }

        dialog {
            width: min(100% - 2rem, 25rem);
            max-height: calc(100% - 2rem);
            padding: 1.25rem;
            color: var(--ink);
            background: var(--surface);
            border: 1px solid var(--line);
            border-radius: .875rem;
        }
        dialog::backdrop { background: rgb(32 23 47 / 52%); }
        dialog h2 { font-size: 1.25rem; }

        @media (max-width: 52rem) {
            .layout { display: block; padding-top: .75rem; }
            .sections { position: static; display: flex; flex-wrap: wrap; gap: .25rem; margin-bottom: 1rem; }
        }
        @media (max-width: 39rem) {
            .wrap { width: calc(100% - 1.5rem); }
            .masthead .wrap { min-height: 4rem; }
            .subsection, .motion-toolbar { padding-inline: .875rem; }
            .two-column, .offset-lists { grid-template-columns: minmax(0, 1fr); }
            .two-column > * + * { border-inline-start: 0; border-top: 1px solid var(--line); padding: 1.25rem 0 0; }
        }
    </style>
</head>

<body>
    <header class="masthead">
        <div class="wrap">
            <h1>Rantoge</h1>
            <a href="/update" class="btn">Update firmware</a>
        </div>
    </header>

    <div class="wrap layout">
        <nav class="sections" aria-label="Settings sections">
            <a href="#motion">Motion</a>
            <a href="#time">Time &amp; date</a>
            <a href="#device">Device</a>
        </nav>
        <main>
            <section id="motion" class="section" aria-labelledby="motion-title">
                <div class="section-title"><h2 id="motion-title">Motion</h2></div>
                <div class="panel">
                    <form class="motion-toolbar" data-command-form aria-label="Clock motion">
                        <div class="actions">
                            <button type="submit" class="btn btn-stop" formaction="/calibrate-end" data-pending="Stopping after the current move…" data-success="Clock paused.">Stop motion</button>
                            <button type="submit" class="btn" formaction="/resume" data-pending="Resuming clock…" data-success="Clock resumed.">Resume clock</button>
                        </div>
                        <span class="motion-state" id="motion-state" role="status"></span>
                        <p class="hint">Stop waits for the current move to finish.</p>
                        <p id="motion-feedback" class="status" role="status"></p>
                        <button type="button" id="check-motion-command" class="btn" hidden>Check motion status</button>
                    </form>

                    <div class="subsection two-column">
                        <div class="stack">
                            <h3>Manual calibration</h3>
                            <p class="hint">Runs continuously. Use Stop to end calibration.</p>
                            <form class="stack" data-command-form aria-label="Manual calibration">
                                <div class="actions">
                                    <button type="submit" class="btn" formaction="/calibrate-hour" data-success="Hour calibration started.">Run hour motor</button>
                                    <button type="submit" class="btn" formaction="/calibrate-minute" data-success="Minute calibration started.">Run minute motor</button>
                                </div>
                                <p class="status" role="status"></p>
                            </form>
                            <h3>Full cycles</h3>
                            <div class="actions">
                                <button type="button" class="btn" data-cycle onclick="fastForward('hours')">Cycle hours</button>
                                <button type="button" class="btn" data-cycle onclick="fastForward('minutes')">Cycle minutes</button>
                            </div>
                            <p id="cycle-status" class="status" role="status"></p>
                            <button type="button" id="check-cycle-command" class="btn" hidden>Check cycle status</button>
                        </div>
                        <div class="stack live-controls">
                            <h3>Adjust displayed digit</h3>
                            <p class="hint">Add 100 microsteps to move forward. Subtract 100 to correct the next arrival.</p>
                            <div class="axis-actions">
                                <span>Hours</span>
                                <button type="button" class="btn" aria-label="Subtract 100 microsteps from the displayed hour" data-offset-command disabled onclick="liveCalibrate('hour', -1)">−100</button>
                                <button type="button" class="btn" aria-label="Add 100 microsteps to the displayed hour" data-offset-command disabled onclick="liveCalibrate('hour', 1)">+100</button>
                            </div>
                            <div class="axis-actions">
                                <span>Minutes</span>
                                <button type="button" class="btn" aria-label="Subtract 100 microsteps from the displayed minute" data-offset-command disabled onclick="liveCalibrate('minute', -1)">−100</button>
                                <button type="button" class="btn" aria-label="Add 100 microsteps to the displayed minute" data-offset-command disabled onclick="liveCalibrate('minute', 1)">+100</button>
                            </div>
                            <p id="live-status" class="status" role="status"></p>
                        </div>
                    </div>

                    <div class="subsection">
                        <h3 id="digit-offsets-title">Digit offsets</h3>
                        <p class="lede hint" id="offset-help">Targets are microsteps per digit. A positive offset advances a late digit. Backward corrections apply on the next arrival.</p>
                        <p id="offset-status" class="status" role="status">Loading motion settings…</p>
                        <button type="button" id="retry-offsets" class="btn" hidden>Reload motion settings</button>
                        <button type="button" id="check-offset-command" class="btn" hidden>Check command status</button>
                        <fieldset id="offsets-container" aria-labelledby="digit-offsets-title" aria-describedby="offset-help" disabled>
                            <div class="offset-lists">
                                <div class="stack">
                                    <label for="hour-target">Hour target
                                        <input type="number" id="hour-target" inputmode="decimal" step="0.01" min="1" required>
                                    </label>
                                    <ul class="offset-list" id="hour-offsets-list" aria-label="Hour offsets"></ul>
                                    <div><button type="button" id="add-hour-offset" class="btn" onclick="showOffsetModal('hour')">Add hour offset</button></div>
                                </div>
                                <div class="stack">
                                    <label for="minute-target">Minute target
                                        <input type="number" id="minute-target" inputmode="decimal" step="0.01" min="1" required>
                                    </label>
                                    <ul class="offset-list" id="minute-offsets-list" aria-label="Minute offsets"></ul>
                                    <div><button type="button" id="add-minute-offset" class="btn" onclick="showOffsetModal('minute')">Add minute offset</button></div>
                                </div>
                            </div>
                            <div class="actions offset-footer">
                                <button type="button" id="save-offsets-btn" class="btn btn-primary" disabled onclick="saveAllOffsets()">Save all offsets</button>
                                <span id="draft-status" class="draft-status" role="status"></span>
                            </div>
                        </fieldset>
                    </div>
                </div>
            </section>

            <section id="time" class="section" aria-labelledby="time-title">
                <div class="section-title"><h2 id="time-title">Time &amp; date</h2></div>
                <div class="panel">
                    <div class="subsection two-column">
                        <form action="/set-displayed" method="post" class="stack" data-command-form aria-labelledby="displayed-title">
                            <h3 id="displayed-title">Displayed time</h3>
                            <p class="hint">Confirm the digits shown on the physical clock after startup or motor calibration, then resume motion.</p>
                            <div class="form-fields">
                                <label for="disp-h">Hour <input type="number" name="h" id="disp-h" min="0" max="23" inputmode="numeric" required></label>
                                <label for="disp-m">Minute <input type="number" name="m" id="disp-m" min="0" max="59" inputmode="numeric" required></label>
                            </div>
                            <div><button type="submit" class="btn" data-success="Displayed time set.">Set displayed time</button></div>
                            <p class="status" role="status"></p>
                        </form>
                        <form action="/set-time" method="post" class="stack" data-command-form aria-labelledby="current-title">
                            <h3 id="current-title">Current time</h3>
                            <p id="time-source" class="hint" role="status"></p>
                            <p class="hint">Setting a manual time pauses network sync. Select network time to enable it again.</p>
                            <div class="form-fields">
                                <label for="time-h">Hour <input type="number" name="h" id="time-h" min="0" max="23" inputmode="numeric" required></label>
                                <label for="time-m">Minute <input type="number" name="m" id="time-m" min="0" max="59" inputmode="numeric" required></label>
                            </div>
                            <div class="actions">
                                <button type="submit" class="btn" data-success="Current time set.">Set current time</button>
                            </div>
                            <p class="status" role="status"></p>
                        </form>
                    </div>
                    <div class="subsection">
                        <form action="/use-ntp" method="post" class="stack" data-command-form aria-label="Network time">
                            <div><button type="submit" class="btn" data-success="Network time selected.">Use network time</button></div>
                            <p class="status" role="status"></p>
                        </form>
                    </div>
                    <div class="subsection two-column">
                        <form action="/set-date" method="post" class="stack" data-command-form aria-labelledby="date-title">
                            <h3 id="date-title">Date</h3>
                            <div class="form-fields date">
                                <label for="date-d">Day <input type="number" name="d" id="date-d" min="1" max="31" inputmode="numeric" required></label>
                                <label for="date-m">Month <input type="number" name="m" id="date-m" min="1" max="12" inputmode="numeric" required></label>
                                <label for="date-y">Year <input type="number" name="y" id="date-y" min="1970" max="9999" inputmode="numeric" required></label>
                            </div>
                            <div><button type="submit" class="btn" data-success="Date set.">Set date</button></div>
                            <p class="status" role="status"></p>
                        </form>
                        <form action="/set-tz" method="post" class="stack" data-command-form aria-labelledby="zone-title">
                            <h3 id="zone-title">Time zone</h3>
                            <label for="tz-name">Time zone name
                                <input type="text" name="tz" id="tz-name" placeholder="Europe/Rome" autocapitalize="off" spellcheck="false" required>
                            </label>
                            <div><button type="submit" class="btn" data-success="Time zone saved.">Save time zone</button></div>
                            <p class="status" role="status"></p>
                        </form>
                    </div>
                </div>
            </section>

            <section id="device" class="section" aria-labelledby="device-title">
                <div class="section-title"><h2 id="device-title">Device</h2></div>
                <div class="panel">
                    <div class="subsection two-column">
                        <form action="/set-wifi" method="post" class="stack" data-command-form aria-labelledby="wifi-title">
                            <h3 id="wifi-title">Wi-Fi network</h3>
                            <label for="wifi-ssid">Network name (SSID)
                                <input type="text" name="ssid" id="wifi-ssid" maxlength="32" autocapitalize="off" spellcheck="false" required>
                            </label>
                            <label for="wifi-password">Password
                                <input type="password" name="password" id="wifi-password" maxlength="64" autocomplete="new-password" aria-describedby="wifi-hint">
                            </label>
                            <p id="wifi-hint" class="hint">Leave blank for an open network. Saving may disconnect this page.</p>
                            <div><button type="submit" class="btn">Save Wi-Fi settings</button></div>
                            <p class="status" role="status"></p>
                        </form>
                        <form action="/set-casio" method="post" class="stack" data-command-form aria-labelledby="beep-title">
                            <h3 id="beep-title">CASIO hourly beep</h3>
                            <fieldset>
                                <legend>Beep schedule</legend>
                                <label class="choice"><input type="radio" name="casio" value="off" id="casio-off" required> Off</label>
                                <label class="choice"><input type="radio" name="casio" value="on" id="casio-on"> All day</label>
                                <label class="choice"><input type="radio" name="casio" value="day" id="casio-day"> 08:00–23:59</label>
                            </fieldset>
                            <label class="choice"><input type="checkbox" name="calibration_hint" id="calibration_hint"> Calibration beep</label>
                            <div><button type="submit" class="btn" data-success="Beep settings saved.">Save beep settings</button></div>
                            <p class="status" role="status"></p>
                        </form>
                    </div>
                    <div class="subsection stack">
                        <h3>Restart and reset</h3>
                        <p class="hint">Factory reset erases all saved settings, including Wi-Fi credentials.</p>
                        <div class="actions">
                            <form action="/reset" method="post" data-command-form onsubmit="return confirm('Restart the clock controller?');">
                                <button type="submit" class="btn">Restart device</button>
                            </form>
                            <form action="/factory-reset" method="post" data-command-form onsubmit="return confirm('Erase all settings, including Wi-Fi credentials, and restart?');">
                                <button type="submit" class="btn btn-danger">Factory reset</button>
                            </form>
                        </div>
                        <p id="system-status" class="status" role="status"></p>
                    </div>
                    <details class="subsection log" id="device-log">
                        <summary>Device log</summary>
                        <p class="hint" id="log-state">Loading recent messages…</p>
                        <pre id="log-messages" tabindex="0" aria-label="Recent device messages"></pre>
                    </details>
                </div>
            </section>
        </main>
    </div>

    <dialog id="offset-modal" aria-labelledby="modal-title">
        <form id="offset-editor" class="stack">
            <h2 id="modal-title">Add offset</h2>
            <label for="modal-index">Digit <select id="modal-index"></select></label>
            <label for="modal-value">Offset (microsteps)
                <input type="number" id="modal-value" step="1" min="-2147483648" max="2147483647" inputmode="numeric" required>
            </label>
            <div class="actions">
                <button type="submit" class="btn btn-primary">Apply to draft</button>
                <button type="button" class="btn" onclick="hideOffsetModal()">Cancel</button>
            </div>
        </form>
    </dialog>

    <script>
        let hourOffsets = new Array(24).fill(0);
        let minuteOffsets = new Array(60).fill(0);
        let currentEdit = { type: '', index: -1 };
        let offsetsChanged = false;
        let offsetsReady = false;
        let motionRevision;
        let pendingOffset;
        let offsetOpener;
        const savedTargets = {};
        let offsetsRevision = 0;
        let savedHourOffsets = new Array(24).fill(0);
        let savedMinuteOffsets = new Array(60).fill(0);

        function positiveMod(value, mod) {
            return ((value % mod) + mod) % mod;
        }

        function normalizeDecimalInput(value) {
            if (typeof value !== 'string') {
                return value;
            }
            return value.replace(',', '.');
        }

        function applyDigitOffset(type, index, newValue) {
            const offsets = type === 'hour' ? hourOffsets : minuteOffsets;
            const modulo = type === 'hour' ? 24 : 60;
            const idx = positiveMod(parseInt(index, 10), modulo);
            offsets[idx] = newValue;
        }

        function mergeSavedOffsets(offsets) {
            // Rebase local, unsaved adjustments on the newly saved calibration.
            // This also preserves edits made while the live request was running.
            const { type } = currentEdit;
            if (type && document.getElementById('offset-modal').open) {
                const index = Number(document.getElementById('modal-index').value);
                const input = document.getElementById('modal-value');
                const value = Number(input.value);
                if (input.value !== '' && Number.isInteger(value)) {
                    const next = type === 'hour' ? offsets.hours : offsets.minutes;
                    const saved = type === 'hour' ? savedHourOffsets : savedMinuteOffsets;
                    input.value = value + next[index] - saved[index];
                }
            }
            hourOffsets = offsets.hours.map((value, i) => value + hourOffsets[i] - savedHourOffsets[i]);
            minuteOffsets = offsets.minutes.map((value, i) => value + minuteOffsets[i] - savedMinuteOffsets[i]);
            savedHourOffsets = offsets.hours.slice();
            savedMinuteOffsets = offsets.minutes.slice();
            renderOffsets();
        }

        function setOffsetRequestBusy(busy) {
            const disabled = busy || !offsetsReady || Boolean(pendingOffset);
            document.getElementById('save-offsets-btn').disabled = disabled;
            document.querySelectorAll('[data-offset-command]').forEach(button => button.disabled = disabled);
        }

        function deleteOffset(type, index) {
            applyDigitOffset(type, index, 0);
            renderOffsets();
            markUnsaved();
        }

        function updateSaveButtonState() {
            document.getElementById('draft-status').textContent = offsetsChanged ? 'Unsaved changes' : '';
        }

        function markUnsaved() {
            ++offsetsRevision;
            offsetsChanged = true;
            updateSaveButtonState();
        }

        function renderOffsets() {
            for (const [type, offsets] of [['hour', hourOffsets], ['minute', minuteOffsets]]) {
                const list = document.getElementById(type + '-offsets-list');
                const focused = list.contains(document.activeElement) ? document.activeElement.id : '';
                list.innerHTML = '';
                offsets.forEach((offset, index) => {
                    if (offset === 0) return;
                    const item = document.createElement('li');
                    item.className = 'offset-item';
                    const digit = String(index).padStart(2, '0');
                    const signed = (offset > 0 ? '+' : '') + offset;
                    item.innerHTML = `<div><strong>${digit}</strong><span class="offset-value">${signed}</span></div>
                        <div class="actions">
                            <button type="button" id="edit-${type}-${index}" class="btn" aria-label="Edit ${type} ${digit} offset" onclick="showOffsetModal('${type}', ${index}, ${offset})">Edit</button>
                            <button type="button" id="delete-${type}-${index}" class="btn" aria-label="Delete ${type} ${digit} offset" onclick="deleteOffset('${type}', ${index})">Delete</button>
                        </div>`;
                    list.appendChild(item);
                });
                if (focused) {
                    const next = document.getElementById(focused) || document.getElementById('add-' + type + '-offset');
                    next.focus();
                }
            }
        }

        function showOffsetModal(type, index = -1, value = 0) {
            offsetOpener = document.activeElement.id;
            currentEdit = { type, index };
            const modal = document.getElementById('offset-modal');
            document.getElementById('modal-title').textContent = (index === -1 ? 'Add ' : 'Edit ') + type + ' Offset';
            
            const indexSelect = document.getElementById('modal-index');
            indexSelect.innerHTML = '';
            const max = type === 'hour' ? 24 : 60;
            for(let i=0; i<max; i++){
                const option = document.createElement('option');
                option.value = i;
                option.textContent = i;
                indexSelect.appendChild(option);
            }

            if (index !== -1) {
                indexSelect.value = index;
                indexSelect.disabled = true;
            } else {
                indexSelect.disabled = false;
            }

            const input = document.getElementById('modal-value');
            input.value = value;
            modal.showModal();
            (index === -1 ? indexSelect : input).focus();
        }

        function hideOffsetModal() {
            document.getElementById('offset-modal').close();
        }

        function saveOffset() {
            const index = parseInt(document.getElementById('modal-index').value, 10);
            const input = document.getElementById('modal-value');
            if (!input.reportValidity()) return;
            applyDigitOffset(currentEdit.type, index, Number(input.value));
            renderOffsets();
            hideOffsetModal();
            markUnsaved();
        }

        async function pollCommand(id, timeout = 120000, boot) {
            const deadline = Date.now() + timeout;
            let failures = 0;
            while (Date.now() < deadline) {
                await new Promise(resolve => setTimeout(resolve, failures ? 750 : 250));
                let result;
                try {
                    const response = await fetch('/command-status?id=' + id + (boot ? '&boot=' + boot : ''), {
                        cache: 'no-store',
                        signal: AbortSignal.timeout(Math.max(1, Math.min(5000, deadline - Date.now())))
                    });
                    if (response.status === 404) {
                        const error = new Error('Command ' + id + ' expired before its result could be confirmed.');
                        error.commandExpired = true;
                        throw error;
                    }
                    if (!response.ok) throw new Error(await response.text());
                    result = await response.json();
                    if (typeof result?.done !== 'boolean') throw new Error('Invalid command status');
                    failures = 0;
                } catch (error) {
                    if (error.commandExpired) throw error;
                    if (++failures >= 3) break;
                    continue;
                }
                if (!result.done) continue;
                if (result.success) return result;
                const error = new Error(result.message);
                error.motionRevision = result.motion_revision;
                throw error;
            }
            const error = new Error('Could not confirm command ' + id + '.');
            error.commandId = id;
            error.commandBoot = boot;
            throw error;
        }

        async function postCommand(url, options, timeout) {
            let response, id, boot;
            try {
                response = await fetch(url, { ...options, signal: AbortSignal.timeout(10000) });
                if (response.status === 202) {
                    ({ id, boot } = await response.json());
                    if (!Number.isInteger(id) || id <= 0) throw new Error('Missing command ID');
                }
            } catch {
                const error = new Error('The device did not confirm receipt. Check its state before retrying.');
                error.submissionUnconfirmed = true;
                throw error;
            }
            if (!response.ok) throw new Error(await response.text());
            if (response.status !== 202) return response.text();
            return pollCommand(id, timeout, boot);
        }

        function showStatus(element, message, state = '') {
            element.textContent = message;
            element.setAttribute('data-state', state);
        }

        let clockBoot;
        let clockStateRevision;
        function updateClockState(state) {
            if (Number.isInteger(state.state_revision)) {
                if (state.boot === clockBoot && state.state_revision < clockStateRevision) return;
                clockBoot = state.boot;
                clockStateRevision = state.state_revision;
            }
            const motion = document.getElementById('motion-state');
            if (state.hour_referenced === false || state.minute_referenced === false) {
                motion.textContent = 'Confirm displayed time';
            } else if (state.time_source === 'none') {
                motion.textContent = 'Waiting for a time source';
            } else if (typeof state.paused === 'boolean') {
                motion.textContent = state.paused ? 'Sync paused' : 'Sync enabled';
            }
            const labels = { ntp: 'Network time', web: 'Manual time', buttons: 'Time set from buttons', none: 'Time source pending' };
            if (labels[state.time_source]) document.getElementById('time-source').textContent = labels[state.time_source];
        }

        async function trackOffsetCommand(operation, onSuccess, onFailure, report) {
            setOffsetRequestBusy(true);
            const check = document.getElementById('check-offset-command');
            check.hidden = true;
            try {
                await onSuccess(await operation());
                pendingOffset = undefined;
            } catch (error) {
                if (error.commandId) {
                    pendingOffset = { id: error.commandId, boot: error.commandBoot, onSuccess, onFailure, report };
                    report(error.message + ' Check its status before making another change.', 'error');
                    check.hidden = false;
                } else {
                    pendingOffset = undefined;
                    if (error.submissionUnconfirmed || error.commandExpired) {
                        offsetsReady = false;
                        report(error.message + ' Reload this page and review the offsets before retrying. Reloading clears the draft.', 'error');
                    } else {
                        onFailure(error);
                    }
                }
            } finally {
                setOffsetRequestBusy(false);
            }
        }

        function checkOffsetCommand() {
            if (!pendingOffset) return;
            const { id, boot, onSuccess, onFailure, report } = pendingOffset;
            report('Checking command status…', 'pending');
            return trackOffsetCommand(() => pollCommand(id, undefined, boot), onSuccess, onFailure, report);
        }

        async function saveAllOffsets() {
            const button = document.getElementById('save-offsets-btn');
            if (button.disabled) return;
            const revision = offsetsRevision;
            const submittedHours = hourOffsets.slice();
            const submittedMinutes = minuteOffsets.slice();
            const formData = new FormData();
            const hourTarget = document.getElementById('hour-target').value;
            const minuteTarget = document.getElementById('minute-target').value;
            formData.append('hour_target', normalizeDecimalInput(hourTarget));
            formData.append('minute_target', normalizeDecimalInput(minuteTarget));
            if (motionRevision) formData.append('motion_revision', motionRevision);
            for (let i = 0; i < 24; i++) formData.append('h' + i, hourOffsets[i]);
            for (let i = 0; i < 60; i++) formData.append('m' + i, minuteOffsets[i]);
            if (!document.getElementById('hour-target').reportValidity() ||
                !document.getElementById('minute-target').reportValidity()) return;
            const report = (message, state) => showStatus(document.getElementById('offset-status'), message, state);
            report('Saving digit offsets…', 'pending');
            await trackOffsetCommand(
                () => postCommand('/set-offsets', { method: 'POST', body: formData }),
                result => {
                    motionRevision = result.motion_revision;
                    savedTargets.hour = hourTarget;
                    savedTargets.minute = minuteTarget;
                    savedHourOffsets = submittedHours;
                    savedMinuteOffsets = submittedMinutes;
                    if (offsetsRevision === revision) offsetsChanged = false;
                    updateSaveButtonState();
                    report('Digit offsets saved.', 'success');
                },
                error => {
                    markUnsaved();
                    if (error.motionRevision && error.motionRevision !== motionRevision) {
                        offsetsReady = false;
                        setOffsetStatus(error.message, true);
                    } else {
                        report('Save failed. ' + error.message, 'error');
                    }
                },
                report
            );
        }

        async function fetchSettings() {
            const response = await fetch('/get-settings', { cache: 'no-store', signal: AbortSignal.timeout(10000) });
            if (!response.ok) throw new Error(await response.text());
            return response.json();
        }

        function setOffsetStatus(message, retry = false) {
            showStatus(document.getElementById('offset-status'), message, retry ? 'error' : '');
            document.getElementById('retry-offsets').hidden = !retry;
        }

        function applyMotionSettings(data) {
            mergeSavedOffsets(data.offsets);
            for (const axis of ['hour', 'minute']) {
                const target = document.getElementById(axis + '-target');
                const previous = savedTargets[axis];
                savedTargets[axis] = (data.base_steps[axis + '_target_x100'] / 100).toFixed(2);
                if (previous === undefined || target.value === previous) target.value = savedTargets[axis];
            }
            motionRevision = data.motion_revision;
        }

        async function loadSettings() {
            const initialized = !document.getElementById('offsets-container').disabled;
            setOffsetStatus('Loading motion settings…');
            try {
                const data = await fetchSettings();
                applyMotionSettings(data);
                if (!document.getElementById('motion-state').textContent) {
                    updateClockState(data);
                }
                if (!initialized) {
                    const ssid = document.getElementById('wifi-ssid');
                    if (!ssid.dataset.edited) ssid.value = data.wifi_ssid || '';
                    const zone = document.getElementById('tz-name');
                    if (!zone.dataset.edited) zone.value = data.tz_name || '';
                    const casio = document.getElementById('casio-' + data.casio_mode);
                    if (casio && !casio.form.dataset.edited) casio.checked = true;
                    const hint = document.getElementById('calibration_hint');
                    if (!hint.dataset.edited) hint.checked = data.casio_cal_hint;
                    if (data.date) {
                        if (data.max_year) document.getElementById('date-y').max = data.max_year;
                        for (const [key, id] of [['day', 'd'], ['month', 'm'], ['year', 'y']]) {
                            const input = document.getElementById('date-' + id);
                            if (!input.dataset.edited) input.value = data.date[key];
                        }
                    }
                    document.getElementById('offsets-container').disabled = false;
                }
                offsetsReady = true;
                setOffsetStatus('');
            } catch (error) {
                setOffsetStatus('Could not load motion settings: ' + error.message, true);
            } finally {
                setOffsetRequestBusy(false);
            }
        }

        function formFeedback(form) {
            if (form.classList.contains('motion-toolbar')) return document.getElementById('motion-feedback');
            return form.querySelector('[role="status"]') || document.getElementById('system-status');
        }

        const latestFormCommands = new WeakMap();

        async function submitForm(event) {
            if (event.defaultPrevented) return;
            event.preventDefault();
            const form = event.currentTarget;
            const button = event.submitter || form.querySelector('button[type="submit"]');
            const action = button?.getAttribute('formaction') || form.getAttribute('action');
            if (!action || !button || button.disabled) return;
            const feedback = formFeedback(form);
            const data = new FormData(form);
            const command = {};
            latestFormCommands.set(feedback, command);
            const motionControl = action === '/resume' || action === '/calibrate-end';
            const check = motionControl && document.getElementById('check-motion-command');
            button.disabled = true;
            const confirmCommand = async (operation, message) => {
                if (check) check.hidden = true;
                showStatus(feedback, message, 'pending');
                try {
                    const result = await operation();
                    if (latestFormCommands.get(feedback) !== command) return;
                    if (typeof result === 'object') updateClockState(result);
                    showStatus(feedback, button.dataset.success || result, 'success');
                } catch (error) {
                    if (latestFormCommands.get(feedback) !== command) return;
                    if (motionControl && (error.commandId || error.commandExpired || error.submissionUnconfirmed)) {
                        document.getElementById('motion-state').textContent = 'Sync status unconfirmed';
                    }
                    showStatus(feedback, error.message, 'error');
                    if (check && error.commandId) {
                        check.hidden = false;
                        check.onclick = () => confirmCommand(() => pollCommand(error.commandId, undefined, error.commandBoot), 'Checking motion status…');
                    }
                }
            };
            await confirmCommand(() => postCommand(action, { method: 'POST', body: data }),
                                 button.dataset.pending || 'Sending command…');
            button.disabled = false;
        }

        document.addEventListener('DOMContentLoaded', () => {
            document.getElementById('hour-target').addEventListener('input', markUnsaved);
            document.getElementById('minute-target').addEventListener('input', markUnsaved);
            document.querySelectorAll('[data-command-form]').forEach(form => {
                form.addEventListener('submit', submitForm);
                form.addEventListener('input', event => {
                    event.target.dataset.edited = 'true';
                    form.dataset.edited = 'true';
                });
            });
            document.getElementById('offset-editor').addEventListener('submit', event => {
                event.preventDefault();
                saveOffset();
            });
            document.getElementById('offset-modal').addEventListener('close', () => {
                const target = document.getElementById(offsetOpener) ||
                               document.getElementById('add-' + currentEdit.type + '-offset');
                target?.focus();
            });
            document.getElementById('retry-offsets').addEventListener('click', loadSettings);
            document.getElementById('check-offset-command').addEventListener('click', checkOffsetCommand);
            loadSettings();
        });

        async function liveCalibrate(type, direction) {
            if (document.getElementById('save-offsets-btn').disabled) return;
            const data = new FormData();
            data.append('type', type);
            data.append('direction', direction);
            const report = (message, state) => showStatus(document.getElementById('live-status'), message, state);
            report('Adjusting the displayed ' + type + '…', 'pending');
            await trackOffsetCommand(
                () => postCommand('/live-calibrate', { method: 'POST', body: data }),
                async () => {
                    try {
                        applyMotionSettings(await fetchSettings());
                        if (pendingOffset) setOffsetStatus('');
                        report('Digit adjustment saved.', 'success');
                    } catch {
                        offsetsReady = false;
                        report('Adjustment saved. Reload motion settings before another change.', 'error');
                        setOffsetStatus('Calibration applied. Reload motion settings before another change.', true);
                    }
                },
                error => report('Calibration failed. ' + error.message, 'error'),
                (message, state) => {
                    report(message, state);
                    showStatus(document.getElementById('offset-status'), message, state);
                }
            );
        }

        async function fastForward(type, id, boot) {
            const buttons = document.querySelectorAll('[data-cycle]');
            const check = document.getElementById('check-cycle-command');
            buttons.forEach(button => button.disabled = true);
            check.hidden = true;
            const feedback = document.getElementById('cycle-status');
            showStatus(feedback, id ? 'Checking cycle status…' : 'Cycling ' + type + '…', 'pending');
            try {
                const timeout = 12 * 60 * 1000;
                if (id) await pollCommand(id, timeout, boot);
                else await postCommand('/fast-forward-' + type, { method: 'POST' }, timeout);
                showStatus(feedback, 'Cycle complete.', 'success');
            } catch (error) {
                showStatus(feedback, error.message, 'error');
                if (error.commandId) {
                    check.hidden = false;
                    check.onclick = () => fastForward(type, error.commandId, error.commandBoot);
                }
            } finally {
                buttons.forEach(button => button.disabled = !check.hidden);
            }
        }

        let refreshing = false;
        async function refreshDeviceState() {
            if (document.hidden || refreshing) return;
            refreshing = true;
            try {
                const response = await fetch('/clock-state', { cache: 'no-store', signal: AbortSignal.timeout(2000) });
                if (response.ok) updateClockState(await response.json());
                if (!document.getElementById('device-log').open) return;
                const logResponse = await fetch('/device-log', { cache: 'no-store', signal: AbortSignal.timeout(2000) });
                if (!logResponse.ok) throw new Error('Log unavailable');
                const log = document.getElementById('log-messages');
                const atEnd = log.scrollHeight - log.scrollTop - log.clientHeight < 12;
                const text = await logResponse.text();
                if (text !== log.textContent) {
                    log.textContent = text;
                    if (atEnd) log.scrollTop = log.scrollHeight;
                }
                document.getElementById('log-state').textContent = text ? '' : 'No recent messages.';
            } catch {
                document.getElementById('log-state').textContent = 'Device unavailable. Retrying…';
            } finally {
                refreshing = false;
            }
        }

        if (window.setInterval) window.setInterval(refreshDeviceState, 2500);
    </script>
</body>

</html>
)rawliteral";
