const char CONFIG_HTML[] PROGMEM = R"rawliteral(

<html>

<head>
    <title>Clock config</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <style>
        html {
            font-family: Arial;
            display: inline-block;
            text-align: center;
        }

        p,
        input {
            font-size: 1.2rem;
            margin-top: 0;
        }

        body {
            margin: 0;
        }

        .topnav {
            overflow: hidden;
            background-color: #666;
            color: white;
            font-size: 1rem;
        }

        .content {
            padding: 20px;
        }

        .card,
        .log {
            background-color: white;
            box-shadow: 2px 2px 12px 1px rgba(140, 140, 140, .5);
        }

        .cards {
            max-width: 800px;
            margin: 0 auto;
            display: grid;
            grid-gap: 2rem;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
        }

        .card form {
            text-align: left;
            padding: 0 1rem;
        }

        .card form.col2 {
            display: grid;
            grid-gap: 0 1rem;
            grid-template-columns: 1fr 1fr;
        }

        .card form.col3 {
            display: grid;
            grid-gap: 0 1rem;
            grid-template-columns: 1fr 1fr 1fr;
        }

        .card form.colx {
            display: block;
            grid-gap: 0 1rem;
            grid-template-columns: 1fr 1fr 1fr;
        }

        .card form label,
        .card form input {
            display: block;
            width: 100%;
        }

        input {
            padding: 0.25rem;
        }

        .card form p.submit {
            grid-column: 1 / -1;
        }

        .log {
            grid-column: 1 / -1;
            padding: 0 1rem;
        }

        .log pre {
            text-align: left;
            ;
        }
    </style>
</head>

<body>
    <div class="topnav">
        <h1>Clock config</h1>
    </div>
    <div class="content">
        <div class="cards">
            <div class="card">
                <h2>Displayed time $TEST_VAR$</h2>
                <form action="/set-displayed" method="post" class="col2">
                    <p><label for="h">Hour</label><input type="text" name="h" id="h" /></p>
                    <p><label for="m">Minute</label><input type="text" name="m" id="m" /></p>
                    <p class="submit"><input type="submit" value="Set" /></p>
                </form>
            </div>

            <div class="card">
                <h2>Wi-Fi network</h2>
                <form action="/set-wifi" method="post">
                    <p><label for="ssid">SSID</label><input type="text" name="ssid" id="ssid" /></p>
                    <p><label for="password">Password</label><input type="text" name="password" id="password" /></p>
                    <p><input type="submit" value="Save" /></p>
                </form>
            </div>

            <div class="card">
                <h2>Current time</h2>
                <form action="/set-time" method="post" class="col2">
                    <p><label for="h">Hour</label><input type="text" name="h" id="h" /></p>
                    <p><label for="m">Minute</label><input type="text" name="m" id="m" /></p>
                    <p class="submit"><input type="submit" value="Set" /></p>
                </form>
            </div>
            <div class="card">
                <h2>Date</h2>
                <form action="/set-date" method="post" class="colx">
                    <p><label for="d">Day</label><input type="text" name="d" id="d" /></p>
                    <p><label for="m">Month</label><input type="text" name="m" id="m" /></p>
                    <p><label for="y">Year</label><input type="text" name="y" id="y" /></p>
                    <p class="submit"><input type="submit" value="Set" /></p>
                </form>
            </div>

            <div class="card">
                <h2>Time zone</h2>
                <form action="/set-tz" method="post">
                    <p><label for="tz">IANA name or POSIX string</label><input type="text" name="tz" id="tz" /></p>
                    <p><input type="submit" value="Save" /></p>
                </form>
            </div>

            <div class="card">
                <h2>Calibrate</h2>
                <form action="/calibrate-hour" method="post">
                    <p><input type="submit" value="Hour" /></p>
                </form>
                <form action="/calibrate-minute" method="post">
                    <p><input type="submit" value="Minute" /></p>
                </form>
                <form action="/calibrate-end" method="post">
                    <p><input type="submit" value="Stop" /></p>
                </form>
            </div>

            <div class="card">
                <h2>Demo</h2>
                <form action="/toggle-demo" method="post">
                    <p><input type="submit" value="Toggle" /></p>
                </form>
            </div>

            <div class="card">
                <h2>CASIO hourly beep</h2>
                <form action="/set-casio" method="post" class="colx">
                    <p><label for="off">Off</label><input type="radio" name="casio" value="off" id="off" /></p>
                    <p><label for="on">On</label><input type="radio" name="casio" value="on" id="on" /></p>
                    <p><label for="day">From 08 to 00</label><input type="radio" name="casio" value="day" id="day" /></p>
                    <p class="submit"><input type="submit" value="Save" /></p>
                </form>
            </div>

            <div class="log">
                <h2>Messages</h2>
                <pre id="log-messages"></pre>
            </div>
        </div>
    </div>



    <script>
        document.addEventListener('DOMContentLoaded', function () {
            document.querySelectorAll('form').forEach(form => {
                form.addEventListener('submit', function (event) {
                    var data = this;
                    fetch(data.getAttribute('action'), {
                        method: data.getAttribute('method'),
                        body: new FormData(data)
                    }).then(res => res.text())
                        .then(function (data) {

                        });
                    event.preventDefault();
                })
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