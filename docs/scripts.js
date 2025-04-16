// Hamburger menu functionality
document.addEventListener('DOMContentLoaded', function() {
    const hamburger = document.querySelector('.hamburger-menu');
    const navLinks = document.querySelector('.nav-links');
    const navContainer = document.querySelector('.nav-container');
    const body = document.body;
    
    hamburger.addEventListener('click', function() {
      navLinks.classList.toggle('active');
      navContainer.classList.toggle('menu-open');
      
      // If menu becomes active, add no-scroll to body
      if (navLinks.classList.contains('active')) {
        body.style.overflow = 'hidden';
      } else {
        body.style.overflow = '';
      }
    });
    
    // Close menu when clicking a link
    const links = document.querySelectorAll('.nav-links a');
    links.forEach(link => {
      link.addEventListener('click', function() {
        navLinks.classList.remove('active');
        navContainer.classList.remove('menu-open');
        body.style.overflow = '';
      });
    });
  
    // Initialize battery demo
    initBatteryDemo();
  });
  
  function toggleDarkMode() {
    const body = document.body;
    const modeIcon = document.querySelector('.mode-icon');
    const modeText = document.querySelector('.mode-text');
    
    body.classList.toggle('dark-mode');
    
    if (body.classList.contains('dark-mode')) {
      modeIcon.textContent = '☀️';
      modeText.textContent = 'Light Mode';
    } else {
      modeIcon.textContent = '🌙';
      modeText.textContent = 'Dark Mode';
    }
  }
  
  // Battery gauge demo functionality
  function initBatteryDemo() {
    // For demo purposes - in real implementation this would be updated with actual battery data
    const batteryLevels = [85, 55, 35, 15];
    let currentIndex = 0;
    
    function updateBatteryDemo() {
      const level = batteryLevels[currentIndex];
      const batteryEl = document.querySelector('.battery-level');
      const textEl = document.querySelector('.battery-text');
      
      if (!batteryEl || !textEl) return; // Guard clause if elements don't exist
      
      batteryEl.style.width = level + '%';
      textEl.textContent = level + '%';
      
      // Update color based on level
      batteryEl.classList.remove('medium', 'low');
      if (level < 25) {
        batteryEl.classList.add('low');
      } else if (level < 50) {
        batteryEl.classList.add('medium');
      }
      
      // Cycle through demo levels
      currentIndex = (currentIndex + 1) % batteryLevels.length;
    }
    
    // Update every 3 seconds for demo
    setInterval(updateBatteryDemo, 3000);
    updateBatteryDemo(); // Initial update
  }