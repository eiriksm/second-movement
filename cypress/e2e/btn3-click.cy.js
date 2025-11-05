describe('Button #btn3 Click Test', () => {
  it('should click button #btn3', () => {
    cy.visit('build-sim/firmware.html')
    cy.wait(5000)
    cy.get('#btn3').click()
    cy.wait(15000)
    cy.get('.not-exist')
  })
})
