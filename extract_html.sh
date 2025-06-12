#!/bin/bash
sed -n '/R"rawliteral(/,/^)rawliteral";/ { /R"rawliteral(/d; /^)rawliteral";/d; p; }' include/HTML.h > test.html
