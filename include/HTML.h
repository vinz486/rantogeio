const char CONFIG_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="it">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Clock config</title>

    <style>
        /* Reset e Base */
        * {
            box-sizing: border-box;
        }
        
        html {
            font-size: 100%;
            -webkit-text-size-adjust: 100%; /* Previene lo zoom del testo su iOS */
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background-color: #f5f5f5;
            margin: 0;
            padding: 0;
            line-height: 1.6;
            color: #333;
            font-size: 1rem;
        }
        
        /* Navigation */
        .topnav {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 1rem 2rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        
        .topnav h1 {
            margin: 0;
            font-size: 1.8rem;
            font-weight: 600;
        }
        
        .topnav a {
            color: white;
            text-decoration: none;
            font-size: 1rem;
            padding: 0.5rem 1rem;
            border: 1px solid rgba(255,255,255,0.3);
            border-radius: 4px;
            transition: all 0.3s ease;
        }
        
        .topnav a:hover {
            background-color: rgba(255,255,255,0.1);
            border-color: rgba(255,255,255,0.5);
        }
        
        /* Content Layout */
        .content {
            padding: 2rem;
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .cards {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 1.5rem;
        }
        
        /* Card Styling */
        .card {
            background: white;
            border-radius: 12px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.08);
            padding: 2rem;
            transition: transform 0.3s ease, box-shadow 0.3s ease;
            border: 1px solid #e1e5e9;
        }
        
        .card:hover {
            transform: translateY(-2px);
            box-shadow: 0 8px 30px rgba(0,0,0,0.12);
        }
        
        .card h2 {
            margin: 0 0 1.5rem 0;
            color: #2c3e50;
            font-size: 1.4rem;
            font-weight: 600;
            border-bottom: 2px solid #e8eaed;
            padding-bottom: 0.75rem;
        }
        
        .card h3 {
            margin: 1.5rem 0 1rem 0;
            color: #34495e;
            font-size: 1.1rem;
            font-weight: 500;
        }
        
        /* Form Elements */
        .form-group {
            margin-bottom: 1.5rem;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 0.5rem;
            font-weight: 500;
            color: #374151;
            font-size: 1.1rem;
        }
        
        /* Input Fields */
        .form-group input[type="text"],
        .form-group input[type="password"],
        .form-group input[type="number"],
        .form-group select {
            width: 100%;
            padding: 1rem;
            border: 2px solid #e5e7eb;
            border-radius: 6px;
            font-size: 1.1rem;
            transition: border-color 0.3s ease, box-shadow 0.3s ease;
            background-color: #fafafa;
            box-sizing: border-box;
        }
        
        .form-group input[type="text"]:focus,
        .form-group input[type="password"]:focus,
        .form-group input[type="number"]:focus,
        .form-group select:focus {
            outline: none;
            border-color: #333;
            box-shadow: 0 0 0 1px #333;
            background-color: white;
        }

        /* Buttons */
        .btn {
            display: inline-block;
            width: 100%;
            padding: 1rem;
            font-size: 1.1rem;
            font-weight: 600;
            color: white;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            border: none;
            border-radius: 6px;
            cursor: pointer;
            transition: all 0.3s ease;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            text-align: center;
            text-decoration: none;
        }

        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 10px rgba(0,0,0,0.15);
            color: white;
        }

        .btn:active {
            transform: scale(0.98);
            filter: brightness(0.95);
        }

        .btn-small {
            padding: 0.5rem 0.8rem;
            font-size: 0.9rem;
            width: auto;
        }

        .btn-auto {
            width: auto;
        }

        .btn-unsaved {
            background: linear-gradient(135deg, #e53935 0%, #b71c1c 100%);
            animation: pulse 1.5s infinite;
        }

        @keyframes pulse {
            0% { box-shadow: 0 0 0 0 rgba(229, 57, 53, 0.7); }
            70% { box-shadow: 0 0 0 10px rgba(229, 57, 53, 0); }
            100% { box-shadow: 0 0 0 0 rgba(229, 57, 53, 0); }
        }
        
        /* Radio Buttons */
        .form-group input[type="radio"] {
            width: auto;
            margin-right: 0.8rem;
            transform: scale(1.5);
        }
        
        .form-group label:has(input[type="radio"]) {
            display: flex;
            align-items: center;
            font-weight: normal;
            margin-bottom: 0.75rem;
            cursor: pointer;
            padding: 0.5rem;
            border-radius: 4px;
            transition: background-color 0.2s ease;
        }
        
        .form-group label:has(input[type="radio"]):hover {
            background-color: #f8f9fa;
        }
        
        /* Form Rows */
        .form-row {
            display: flex;
            gap: 1rem;
            margin-bottom: 1.5rem;
        }
        
        .form-row .form-group {
            flex: 1;
            margin-bottom: 0;
            min-width: 0;
        }
        
        /* Lists */
        ul {
            list-style: none;
            padding: 0;
            margin: 0;
        }
        
        .offset-item {
            display: flex;
            gap: 1rem;
            align-items: center;
            padding: 1rem;
            margin-bottom: 0.5rem;
            background-color: #f8f9fa;
            border-radius: 6px;
            border: 1px solid #e9ecef;
        }
        
        .offset-item > div:first-child {
            display: flex;
            gap: 1rem;
            align-items: center;
            flex-grow: 1;
        }
        .offset-value {
            font-family: monospace;
            white-space: pre;
            text-align: right;
            min-width: 4ch;
        }
        .btn-delete {
            /* margin-left: 0.5rem; */
        }

        .offset-actions {
            display: flex;
            gap: 0.5rem;
            flex-wrap: nowrap;
        }
        
        
        /* Log Card */
        .log {
            grid-column: 1 / -1;
        }
        
        #log-messages {
            background-color: #1e1e1e;
            color: #e0e0e0;
            padding: 1rem;
            border-radius: 6px;
            font-family: 'Courier New', monospace;
            font-size: 0.875rem;
            max-height: 300px;
            overflow-y: auto;
            white-space: pre-wrap;
            margin: 0;
        }
        
        /* Modal */
        #offset-modal {
            display: none;
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: rgba(0, 0, 0, 0.6);
            z-index: 1000;
        }
        
        #offset-modal.show {
            display: flex;
            justify-content: center;
            align-items: center;
        }
        
        #offset-modal > div.modal-content {
            background: white;
            padding: 2rem;
            border-radius: 12px;
            width: 90%;
            max-width: 450px;
            max-height: 80vh;
            overflow-y: auto;
            box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
        }
        
        #offset-modal > div.modal-content h2 {
            margin-top: 0;
            margin-bottom: 1.5rem;
            color: #2c3e50;
        }
        
        /* Responsive Design */
        @media (max-width: 768px) {
            .topnav {
                flex-direction: column;
                gap: 1rem;
                text-align: center;
            }
            
            .content {
                padding: 1rem;
            }
            
            .cards {
                grid-template-columns: 1fr;
                gap: 1rem;
            }
            
            .card {
                padding: 1.5rem;
            }
            
            .form-row {
                flex-direction: column;
                gap: 1rem;
            }

            .card form {
                display: flex;
                flex-direction: column;
                gap: 1rem;
            }
        }
        
>>>>>>> REPLACE
    </style>
</head>

<body>
    <div class="topnav">
        <h1>Clock Config</h1>
        <a href="/update" class="btn btn-auto">Update Firmware</a>
    </div>
    <div class="content">
        <div class="cards">

            <div class="card">
                <h2>Displayed Time</h2>
                <form action="/set-displayed" method="post">
                    <div class="form-group time-input">
                        <label for="disp-h">Hour</label>
                        <input type="text" name="h" id="disp-h" />
                    </div>
                    <div class="form-group time-input">
                        <label for="disp-m">Minute</label>
                        <input type="text" name="m" id="disp-m" />
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Set</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Wi-Fi Network</h2>
                <form action="/set-wifi" method="post">
                    <div class="form-group">
                        <label for="wifi-ssid">SSID</label>
                        <input type="text" name="ssid" id="wifi-ssid" />
                    </div>
                    <div class="form-group">
                        <label for="wifi-password">Password</label>
                        <input type="password" name="password" id="wifi-password" />
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Save</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Current Time</h2>
                <form action="/set-time" method="post">
                    <div class="form-group time-input">
                        <label for="time-h">Hour</label>
                        <input type="text" name="h" id="time-h" />
                    </div>
                    <div class="form-group time-input">
                        <label for="time-m">Minute</label>
                        <input type="text" name="m" id="time-m" />
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Set</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Date</h2>
                <form action="/set-date" method="post">
                    <div class="form-group date-input">
                        <label for="date-d">Day</label>
                        <input type="text" name="d" id="date-d" />
                    </div>
                    <div class="form-group date-input">
                        <label for="date-m">Month</label>
                        <input type="text" name="m" id="date-m" />
                    </div>
                    <div class="form-group year-input">
                        <label for="date-y">Year</label>
                        <input type="text" name="y" id="date-y" />
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Set</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Time Zone</h2>
                <form action="/set-tz" method="post">
                    <div class="form-group">
                        <label for="tz-name">IANA Name or POSIX String</label>
                        <input type="text" name="tz" id="tz-name" />
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Save</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>CASIO Hourly Beep</h2>
                <form action="/set-casio" method="post">
                    <div class="form-group">
                        <label><input type="radio" name="casio" value="off" id="casio-off"> Off</label>
                        <label><input type="radio" name="casio" value="on" id="casio-on"> On</label>
                        <label><input type="radio" name="casio" value="day" id="casio-day"> From 08 to 00</label>
                    </div>
                    <div class="form-group">
                        <label style="flex-direction: row; align-items: center; gap: 1rem;">
                            <input type="checkbox" name="calibration_hint" id="calibration_hint" style="transform: scale(1.5);">
                            Calibration hint
                        </label>
                    </div>
                    <div class="form-group">
                        <button type="submit" class="btn">Save</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Calibrate</h2>
                <form>
                    <div class="form-row">
                        <div class="form-group">
                             <button type="submit" class="btn" formaction="/calibrate-hour" formmethod="post">Hour</button>
                        </div>
                        <div class="form-group">
                            <button type="submit" class="btn" formaction="/calibrate-minute" formmethod="post">Minute</button>
                        </div>
                        <div class="form-group">
                            <button type="submit" class="btn" formaction="/calibrate-end" formmethod="post">Stop</button>
                        </div>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Demo</h2>
                <form action="/toggle-demo" method="post">
                    <div class="form-group">
                        <button type="submit" class="btn">Toggle</button>
                    </div>
                </form>
            </div>

            <div class="card">
                <h2>Stepper Offsets</h2>
                <div id="offsets-container">
                    <h3>Hour Offsets</h3>
                    <div class="form-group">
                        <label for="base-hour">Base Steps Hour:</label>
                        <input type="number" id="base-hour" placeholder="492" />
                    </div>
                    <ul id="hour-offsets-list"></ul>
                    <button type="button" class="btn" onclick="showOffsetModal('hour')">Add Hour Offset</button>

                    <h3>Minute Offsets</h3>
                    <div class="form-group">
                        <label for="base-minute">Base Steps Minute:</label>
                        <input type="number" id="base-minute" placeholder="1177" />
                    </div>
                    <ul id="minute-offsets-list"></ul>
                    <button type="button" class="btn" onclick="showOffsetModal('minute')">Add Minute Offset</button>

                    <hr style="margin: 2rem 0; border: none; border-top: 1px solid #e8eaed;">

                    <div class="form-group" style="margin-top: 1rem;">
                        <button type="button" id="save-offsets-btn" class="btn" onclick="saveAllOffsets()">Save All Offsets</button>
                    </div>
                </div>
            </div>

            <div class="card log">
                <h2>Messages</h2>
                <pre id="log-messages"></pre>
            </div>

        </div>
    </div>

    <div id="offset-modal">
        <div class="modal-content">
            <h2 id="modal-title">Add Offset</h2>
            <div class="form-group">
                <label for="modal-index">Index:</label>
                <select id="modal-index"></select>
            </div>
            <div class="form-group">
                <label for="modal-value">Offset Value:</label>
                <input type="number" id="modal-value" />
            </div>
            <div class="form-row">
                <div class="form-group">
                    <button type="button" class="btn" onclick="saveOffset()">Save</button>
                </div>
                <div class="form-group">
                    <button type="button" class="btn" onclick="hideOffsetModal()">Cancel</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let hourOffsets = new Array(24).fill(0);
        let minuteOffsets = new Array(60).fill(0);
        let currentEdit = { type: '', index: -1 };
        let offsetsChanged = false;

        function updateSaveButtonState() {
            const saveBtn = document.getElementById('save-offsets-btn');
            if (offsetsChanged) {
                saveBtn.classList.add('btn-unsaved');
            } else {
                saveBtn.classList.remove('btn-unsaved');
            }
        }

        function markUnsaved() {
            offsetsChanged = true;
            updateSaveButtonState();
        }

        function renderOffsets() {
            const hourList = document.getElementById('hour-offsets-list');
            hourList.innerHTML = '';
            hourOffsets.forEach((offset, index) => {
                if (offset !== 0) {
                    const li = document.createElement('li');
                    li.className = 'offset-item';
                    const signedOffset = (offset > 0 ? '+' : '') + offset;
                    li.innerHTML = `<div><span class="offset-label">H ${index.toString().padStart(2, '0')}</span><span class="offset-value">${signedOffset}</span></div>` +
                                 '<div class="offset-actions">' +
                                 `<button class="btn btn-small" onclick="showOffsetModal('hour', ${index}, ${offset})">Edit</button>` +
                                 `<button class="btn btn-small btn-delete" onclick="hourOffsets[${index}] = 0; renderOffsets(); markUnsaved();">Delete</button>` +
                                 '</div>';
                    hourList.appendChild(li);
                }
            });

            const minuteList = document.getElementById('minute-offsets-list');
            minuteList.innerHTML = '';
            minuteOffsets.forEach((offset, index) => {
                if (offset !== 0) {
                    const li = document.createElement('li');
                    li.className = 'offset-item';
                    const signedOffset = (offset > 0 ? '+' : '') + offset;
                    li.innerHTML = `<div><span class="offset-label">M ${index.toString().padStart(2, '0')}</span><span class="offset-value">${signedOffset}</span></div>` +
                                 '<div class="offset-actions">' +
                                 `<button class="btn btn-small" onclick="showOffsetModal('minute', ${index}, ${offset})">Edit</button>` +
                                 `<button class="btn btn-small btn-delete" onclick="minuteOffsets[${index}] = 0; renderOffsets(); markUnsaved();">Delete</button>` +
                                 '</div>';
                    minuteList.appendChild(li);
                }
            });
        }

        function showOffsetModal(type, index = -1, value = 0) {
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

            document.getElementById('modal-value').value = value;
            modal.classList.add('show');
        }

        function hideOffsetModal() {
            const modal = document.getElementById('offset-modal');
            modal.classList.remove('show');
        }

        function saveOffset() {
            const index = document.getElementById('modal-index').value;
            const value = parseInt(document.getElementById('modal-value').value, 10);
            if (currentEdit.type === 'hour') {
                hourOffsets[index] = value;
            } else {
                minuteOffsets[index] = value;
            }
            renderOffsets();
            hideOffsetModal();
            markUnsaved();
        }

        function saveAllOffsets() {
            const formData = new FormData();
            
            // Add base steps
            const baseHour = document.getElementById('base-hour').value;
            const baseMinute = document.getElementById('base-minute').value;
            if (baseHour !== '') formData.append('base_hour', baseHour);
            if (baseMinute !== '') formData.append('base_minute', baseMinute);
            
            // Add offsets
            for(let i=0; i<24; i++) {
                formData.append('h' + i, hourOffsets[i]);
            }
            for(let i=0; i<60; i++) {
                formData.append('m' + i, minuteOffsets[i]);
            }

            fetch('/set-offsets', {
                method: 'POST',
                body: formData
            }).then(res => {
                if (res.ok) {
                    offsetsChanged = false;
                    updateSaveButtonState();
                }
                return res.text();
            }).then(console.log);
        }

        document.addEventListener('DOMContentLoaded', function () {
            // Add listeners for base steps to mark as unsaved
            document.getElementById('base-hour').addEventListener('input', markUnsaved);
            document.getElementById('base-minute').addEventListener('input', markUnsaved);

            document.querySelectorAll('form').forEach(form => {
                form.addEventListener('submit', function (event) {
                    if (this.id !== 'offsets-form') {
                        event.preventDefault();
                        const form = this;
                        const submitter = event.submitter;

                        const action = submitter ? (submitter.getAttribute('formaction') || form.getAttribute('action')) : form.getAttribute('action');
                        const method = submitter ? (submitter.getAttribute('formmethod') || form.getAttribute('method')) : form.getAttribute('method');

                        if (!action) {
                            console.error("Form submission failed: no action specified on form or submitter.");
                            return;
                        }

                        fetch(action, {
                            method: method || 'post',
                            body: new FormData(form)
                        }).then(res => res.text())
                            .then(data => {
                                console.log(data);
                            });
                    }
                })
            });

            fetch('/get-settings')
                .then(response => response.json())
                .then(data => {
                    // Base steps
                    if(data.base_steps) {
                        document.getElementById('base-hour').value = data.base_steps.hour;
                        document.getElementById('base-minute').value = data.base_steps.minute;
                    }

                    // Offsets
                    hourOffsets = data.offsets.hours;
                    minuteOffsets = data.offsets.minutes;
                    renderOffsets();

                    // Other settings
                    if(data.wifi_ssid) document.getElementById('wifi-ssid').value = data.wifi_ssid;
                    if(data.tz_name) document.getElementById('tz-name').value = data.tz_name;
                    if(data.casio_mode) document.getElementById('casio-' + data.casio_mode).checked = true;
                    if(data.casio_cal_hint) document.getElementById('calibration_hint').checked = data.casio_cal_hint;
                    if(data.date) {
                        document.getElementById('date-d').value = data.date.day;
                        document.getElementById('date-m').value = data.date.month;
                        document.getElementById('date-y').value = data.date.year;
                    }
                });
        });

        if (!!window.EventSource) {
            var source = new EventSource('/events');

            source.addEventListener('message', function (e) {
                document.getElementById("log-messages").innerHTML += e.data + "\n";
                console.log("message", e.data);
            }, false);

        }
    </script>
</body>

</html>
)rawliteral";
//)---";
