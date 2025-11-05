# Cypress Tests

This directory contains Cypress end-to-end tests for the second-movement watch simulator.

## Setup

1. Install dependencies:
   ```bash
   npm install
   ```

2. Start the local web server:
   ```bash
   npm start
   ```
   This will serve the project files on http://localhost:8000

## Running Tests

**Note:** Make sure the web server is running (`npm start`) in a separate terminal before running tests.

### Interactive Mode
```bash
npm run cypress:open
# or
npm run test:open
```

### Headless Mode
```bash
npm run cypress:run
# or
npm test
```

## Tests

### btn3-click.cy.js
Tests clicking button #btn3 in the watch simulator interface.

## Configuration

- **cypress.config.js**: Main Cypress configuration
- **cypress/support/**: Support files and custom commands
- **cypress/e2e/**: Test files
- **cypress/fixtures/**: Test data files (if needed)
