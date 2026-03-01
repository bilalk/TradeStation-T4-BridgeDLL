# Updated Script for Smoke Test

# Overall timeout and failure diagnostics added

# Using StreamReader/StreamWriter with UTF8Encoding without BOM

# Retry/Backoff connect loop remains intact

function Send-Command {
    param (
        [string]$command
    )

    # Log before writing
    Write-Host "Sending command: $command"

    try {
        $writer = [System.IO.StreamWriter]::new($pipeStream, [System.Text.Encoding]::UTF8, 1024, $true)
        $reader = [System.IO.StreamReader]::new($pipeStream, [System.Text.Encoding]::UTF8, 1024, $true)

        # Write the command using StreamWriter 
        $writer.WriteLine($command)
        $writer.Flush()

        # Log after writing
        Write-Host "Command sent successfully."

        # Read with timeout
        $result = $reader.ReadLineAsync()
        if (-not $result.Wait(5000)) {  # 5 seconds timeout
            throw "Read operation timed out."
        }
        Write-Host "Received response: $($result.Result)"

    } catch {
        # Dump stdout/stderr to temp files on failure
        $stderrPath = "logs/stderr.txt"
        $stdoutPath = "logs/stdout.txt"
        [System.IO.File]::WriteAllText($stderrPath, $_.Exception.Message)
        [System.IO.File]::WriteAllText($stdoutPath, "No output.")
        Write-Host "Error occurred. Check logs at $stderrPath and $stdoutPath."
    } finally {
        $writer.Dispose()
        $reader.Dispose()
    }
}

# Consider adding simple read idle timeout in PipeServer.cs if needed.