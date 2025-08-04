try {
  console.log('Loading demo...');
  const demo = require('./dist/demo/windows-desktop.js');
  console.log('Demo loaded successfully:', typeof demo);
} catch (error) {
  console.error('Error loading demo:', error.message);
  console.error('Stack:', error.stack);
}
