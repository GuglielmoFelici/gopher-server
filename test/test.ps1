param(
	[string]$path = "./",
	[string]$port = "7070"
)
$outFile="compare_res"
$outDir="out"
Write-Output "Testing files in $path"
if (Test-Path $outFile) {
	Clear-Content $outFile
}
if (!$(Test-Path $outDir)) {
	mkdir $outDir *>$null
}
Get-ChildItem -Path $path -Exclude "out" | ForEach-Object {
	$out = $_
	$isDir = $(Test-Path -Path $path$_ -PathType Container)
	if (Test-Path -Path $path$_ -PathType Container) {
		$_ = "$_/"
	}
	mycurl gopher://localhost:$port//$_ --output out\$out *>$null 
	if (!$isDir) {
		fc.exe /b $path$_ out\$_ 1>>$outFile 2>$null
	}
}
Write-Output "Results are stored in file $outFile"