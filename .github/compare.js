const child_process = require('child_process');

let command = 'magick compare';

if (process.platform === 'linux') {
	command = 'compare-im6';
}
else if (process.platform === 'darwin') {
	command = 'export DYLD_LIBRARY_PATH="$HOME/imagemagick/lib/" && $HOME/imagemagick/bin/magick compare';
}

try {
	child_process.execSync(command + ' -metric mae ./reference.png ./test.png difference.png', {stdio: 'pipe', encoding: 'utf8'});
}
catch (err) {
	if (err.code) {
		console.log('Could not run imagemagick.');
		process.exit(1);
	}
	else {
		const { stdout, stderr } = err;
		const compare = parseFloat(stderr.substring(stderr.indexOf('(') + 1, stderr.indexOf(')')));
		console.log('Compare value is ' + compare + '.');
		if (compare > 0.001) {
			console.log('That\'s not good enough.');
			process.exit(1);
		}
	}
}
