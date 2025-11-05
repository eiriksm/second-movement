const { defineConfig } = require('cypress')

module.exports = defineConfig({
  e2e: {
    screenshot: true,
    screenshotOnRunFailure: true,
    // Configure baseUrl to serve files from the project root
    // You can start a local server with: python3 -m http.server 8000
    baseUrl: 'http://localhost:8000',
    specPattern: 'cypress/e2e/**/*.cy.{js,jsx,ts,tsx}',
    supportFile: 'cypress/support/e2e.js',
    video: false,
    screenshotOnRunFailure: true,
    setupNodeEvents(on, config) {
      on('before:browser:launch', (browser = {}, launchOptions) => {
        if (browser.family === 'chromium') {
          launchOptions.args = launchOptions.args.filter(a => !a.includes('--mute-audio'));
          launchOptions.args.push('--autoplay-policy=no-user-gesture-required'); // harmless even if not needed
        }
        return launchOptions;
      });
    },
  },
})
