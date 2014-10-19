on run eventArgs
    if application "Keynote" is running then
        tell application "Keynote"
            set aF to frontmost
            set aP to playing
            if {aF, aP} = {true, true} then
                show next
            end if
        end tell
    end if
end run
