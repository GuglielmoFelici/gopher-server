param(
	[string]$port = "7070",
	[string]$depth = "1"
)
$compareFile = "test/compare_res"
$outDir = "test/out"
Write-Output "Testing files
Recursion limited to $depth
Port $port"
if (Test-Path $compareFile) {
	Clear-Content $compareFile
}
if (!$(Test-Path $outDir)) {
	mkdir $outDir *>$null
}
$result = $(Get-ChildItem -Recurse -Depth $depth | 
	Where-Object { $_ -notmatch $outDir } | 
	ForEach-Object {
		$relPath = $($_ | Resolve-Path -Relative) -replace '\.\\', ''
		$isDir = $(Test-Path -Path $_.fullname -PathType Container)
		if ($isDir) {
			$relPath = "$relPath/"
		}
		$out = $relPath -replace '[\\/]', '_'
		mycurl gopher://localhost:$port//$relPath --output $outDir/$out *>$null 
		if (!$isDir) {
			$comp = fc.exe /b $relPath $outDir\$out 2>null
			Write-Output $comp | Out-File $compareFile -Append
			return $comp -match '[A-Z\d]{8}: ([A-Z\d]{2} ?){2}'
		}
	}) -notcontains $False
if ($result) {
	Write-Output "All tests successfull!"
}
else {
	Write-Output "Some tests failed"
}
Write-Output "Check comparison results in $compareFile"