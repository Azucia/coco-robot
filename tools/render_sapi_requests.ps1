Add-Type -AssemblyName System.Speech
$lines=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'coco_speech_lines.json') -Encoding UTF8 -Raw | ConvertFrom-Json
$speaker=New-Object System.Speech.Synthesis.SpeechSynthesizer
$speaker.SelectVoice('Microsoft Huihui Desktop')
$format=New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(16000,[System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen,[System.Speech.AudioFormat.AudioChannel]::Mono)
foreach($line in $lines.PSObject.Properties){
    $speaker.SetOutputToWaveFile((Join-Path $PSScriptRoot ($line.Name+'-request.wav')),$format)
    $prefix=-join ([char[]](35831,20320,21482,35828,19979,38754,36825,21477,35805,65292,19981,35201,28155,21152,20854,20182,20869,23481,65306))
    $speaker.Speak($prefix+$line.Value)
    $speaker.SetOutputToNull()
}
$speaker.Dispose()
