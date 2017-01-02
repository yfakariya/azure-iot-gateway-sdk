$latin1 = [Text.Encoding]::GetEncoding("iso-8859-1")
$utf8 = New-Object Text.UTF8Encoding($true)

function ToUtf8([string]$file)
{
    [IO.File]::WriteAllText($file, [IO.File]::ReadAllText($file, $latin1), $utf8)
}

foreach ($file in $Args)
{
	ToUtf8 $file
}