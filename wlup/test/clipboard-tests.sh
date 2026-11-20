# Clipboard tests shared by run-e2e.sh (sway) and run-e2e-mutter.sh
# (GNOME), sourced by them. A foot terminal in the session must have the
# keyboard focus, and DISPLAY must be the RDP client's X display.
#
# The text has two lines and non-ASCII characters, to check the UTF-16
# conversion and the CR LF line ends of the RDP clipboard.
#
# Session to client: foot sets the session's clipboard with OSC 52; the
# RDP client's X clipboard is read with xclip.
# Client to session: the client's X clipboard is set with xclip, and
# pasted into 'cat' in foot with Ctrl+Shift+V.
#
# Results go to $OUT/clip-*.txt; clipboard_checks() checks them.

# 日本 in UTF-8, as octal escapes for printf
CLIP_JA='\346\227\245\346\234\254'

clipboard_tests() {
    # --- session to client
    xdotool type --delay 40 \
        "printf '\\033]52;c;%s\\a' \"\$(printf 'from-session-42\\n$CLIP_JA' | base64 -w0)\""
    xdotool key Return
    sleep 3
    printf "from-session-42\n$CLIP_JA" > "$OUT/clip-s2c-expected.txt"
    timeout 10 xclip -o -selection clipboard > "$OUT/clip-s2c.txt" 2>&1 || true

    # --- client to session
    printf "from-client-42\n$CLIP_JA" | xclip -selection clipboard -i
    sleep 3
    xdotool type --delay 40 'cat > /tmp/pasted.txt'
    xdotool key Return
    sleep 0.5
    xdotool key ctrl+shift+v
    sleep 2
    xdotool key Return ctrl+d
    sleep 1
    printf "from-client-42\n$CLIP_JA\n" > "$OUT/clip-c2s-expected.txt"
    cp /tmp/pasted.txt "$OUT/clip-c2s.txt" 2>/dev/null || true
}

clipboard_checks() {
    check "clipboard: session to client (UTF-8, two lines)" \
        'cmp -s "$OUT/clip-s2c.txt" "$OUT/clip-s2c-expected.txt"'
    check "clipboard: client to session (UTF-8, two lines)" \
        'cmp -s "$OUT/clip-c2s.txt" "$OUT/clip-c2s-expected.txt"'
}
