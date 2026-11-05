$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot=Split-Path -Parent $PSScriptRoot
$output=Join-Path $taskRoot 'source/remote/WallERemote/coco_button_labels.h'
$lines=New-Object 'System.Collections.Generic.List[string]'
$lines.Add('#pragma once')
$lines.Add('#include <stdint.h>')
foreach($entry in @(@('coco_head_label',(-join [char[]](25671,22836))),@('coco_wake_label',(-join [char[]](21796,37266))))){
    $bitmap=New-Object System.Drawing.Bitmap(80,32)
    $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::Black)
    $graphics.TextRenderingHint=[System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $font=New-Object System.Drawing.Font('Microsoft YaHei',22,[System.Drawing.FontStyle]::Bold,[System.Drawing.GraphicsUnit]::Pixel)
    $format=New-Object System.Drawing.StringFormat
    $format.Alignment=[System.Drawing.StringAlignment]::Center
    $format.LineAlignment=[System.Drawing.StringAlignment]::Center
    $rectangle=New-Object System.Drawing.RectangleF(0,0,80,32)
    $graphics.DrawString($entry[1],$font,[System.Drawing.Brushes]::White,$rectangle,$format)
    $bytes=New-Object 'System.Collections.Generic.List[string]'
    for($y=0;$y -lt 32;$y++){
        for($x=0;$x -lt 80;$x+=8){
            $value=0
            for($bit=0;$bit -lt 8;$bit++){if($bitmap.GetPixel($x+$bit,$y).R -gt 127){$value=$value -bor (1 -shl (7-$bit))}}
            $bytes.Add(('0x{0:X2}' -f $value))
        }
    }
    $lines.Add(('static const uint8_t {0}[] = {{' -f $entry[0]))
    for($i=0;$i -lt $bytes.Count;$i+=10){$lines.Add('    '+[string]::Join(',',$bytes.GetRange($i,10))+',')}
    $lines.Add('};')
    $format.Dispose();$font.Dispose();$graphics.Dispose();$bitmap.Dispose()
}
[IO.File]::WriteAllLines($output,$lines,[Text.UTF8Encoding]::new($false))
Write-Output 'Chinese head/wake labels generated'
