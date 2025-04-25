// Main functionality
document.addEventListener('DOMContentLoaded', function() {
  // DOM elements
  const hamburger = document.querySelector('.hamburger-menu');
  const navLinks = document.querySelector('.nav-links');
  const navContainer = document.querySelector('.nav-container');
  const body = document.body;
  const darkModeToggle = document.getElementById('dark-mode-toggle');
  const menuOverlay = document.querySelector('.menu-overlay');
  
  // Hamburger menu functionality
  if (hamburger) {
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
  }
  
  // Close menu when clicking on overlay
  if (menuOverlay) {
    menuOverlay.addEventListener('click', function() {
      navLinks.classList.remove('active');
      navContainer.classList.remove('menu-open');
      body.style.overflow = '';
    });
  }
  
  // Close menu when clicking a link
  const links = document.querySelectorAll('.nav-links a');
  links.forEach(link => {
    link.addEventListener('click', function() {
      navLinks.classList.remove('active');
      navContainer.classList.remove('menu-open');
      body.style.overflow = '';
    });
  });

  // Dark mode functionality
  // Check for saved dark mode preference
  const isDarkMode = localStorage.getItem('darkMode') === 'true';
  
  // Apply saved preference or system preference
  if (isDarkMode || (!localStorage.getItem('darkMode') && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
    document.body.classList.add('dark-mode');
    updateDarkModeIcon(true);
  }
  
  // Dark mode toggle function
  if (darkModeToggle) {
    darkModeToggle.addEventListener('click', function() {
      document.body.classList.toggle('dark-mode');
      const isDark = document.body.classList.contains('dark-mode');
      localStorage.setItem('darkMode', isDark);
      updateDarkModeIcon(isDark);
    });
  }
  
  // Initialize battery demo
  initBatteryDemo();
  
  // Handle escape key to close menu
  document.addEventListener('keydown', function(e) {
    if (e.key === 'Escape' && navLinks.classList.contains('active')) {
      navLinks.classList.remove('active');
      navContainer.classList.remove('menu-open');
      body.style.overflow = '';
    }
  });
});

// Update dark mode icon based on state
function updateDarkModeIcon(isDark) {
  const darkModeToggle = document.getElementById('dark-mode-toggle');
  if (!darkModeToggle) return;
  
  // The SVG content will be replaced
  if (isDark) {
    darkModeToggle.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <circle cx="12" cy="12" r="5"></circle>
      <line x1="12" y1="1" x2="12" y2="3"></line>
      <line x1="12" y1="21" x2="12" y2="23"></line>
      <line x1="4.22" y1="4.22" x2="5.64" y2="5.64"></line>
      <line x1="18.36" y1="18.36" x2="19.78" y2="19.78"></line>
      <line x1="1" y1="12" x2="3" y2="12"></line>
      <line x1="21" y1="12" x2="23" y2="12"></line>
      <line x1="4.22" y1="19.78" x2="5.64" y2="18.36"></line>
      <line x1="18.36" y1="5.64" x2="19.78" y2="4.22"></line>
    </svg>`;
  } else {
    darkModeToggle.innerHTML = `<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M21 12.79A9 9 0 1 1 11.21 3 7 7 0 0 0 21 12.79z"></path>
    </svg>`;
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