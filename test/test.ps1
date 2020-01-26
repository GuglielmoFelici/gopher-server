param(
	[string]$port = "7070",
	[string]$depth = "2"
)
$outFile="compare_res"
$outDir="test/out"
Write-Output "Testing files
Recursion limited to $depth
Port $port"
if (Test-Path $outFile) {
	Clear-Content $outFile
}
if (!$(Test-Path $outDir)) {
	mkdir $outDir *>$null
}
Get-ChildItem -Recurse -Depth $depth | Where-Object {$_ -notmatch $outDir} | ForEach-Object {
	$relPath = $_ | Resolve-Path -Relative
	$out = $relPath -replace '/', '_'
	$isDir = $(Test-Path -Path $_.fullname -PathType Container)
	if ($isDir) {
		$relPath = "$relPath/"
	}
	mycurl gopher://localhost:$port//$relPath --output out\$out *>$null 
	if (!$isDir) {
		fc.exe /b $_ $outDir\$out 1>>$outFile 2>$null
	}
}
Write-Output "Results are stored in file $outFile"