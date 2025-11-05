describe('Button #btn3 Click Test', () => {
  it('should click button #btn3', () => {
    // Visit the watch simulator HTML file
    cy.visit('build-sim/firmware.html')

    // Find and click button #btn3
    cy.wait(5000)
    cy.get('#btn3').click()
    cy.wait(15000)

    // Optional: Add assertions to verify the click had the expected effect
    // For example, you might check if a certain element appears or changes
    // cy.get('#some-element').should('be.visible')
  })
})
