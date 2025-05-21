<script>
  export let type = 'button';
  export let color = 'primary'; // primary, success, danger, warning, neutral
  export let size = 'md'; // sm, md, lg
  export let disabled = false;
  export let loading = false;
  export let fullWidth = false;
  export let icon = '';
  
  const colorClasses = {
    primary: 'bg-purple-600 hover:bg-purple-700 active:bg-purple-800 focus:ring-purple-300',
    success: 'bg-green-600 hover:bg-green-700 active:bg-green-800 focus:ring-green-300',
    danger: 'bg-red-600 hover:bg-red-700 active:bg-red-800 focus:ring-red-300',
    warning: 'bg-yellow-500 hover:bg-yellow-600 active:bg-yellow-700 focus:ring-yellow-300',
    neutral: 'bg-gray-500 hover:bg-gray-600 active:bg-gray-700 focus:ring-gray-300'
  };
  
  const sizeClasses = {
    sm: 'px-3 py-1 text-sm',
    md: 'px-4 py-2',
    lg: 'px-5 py-3 text-lg'
  };
</script>

<button
  {type}
  class={`text-white font-medium rounded-lg focus:outline-none focus:ring-4 transition-colors duration-200 disabled:opacity-50 disabled:cursor-not-allowed ${colorClasses[color]} ${sizeClasses[size]} ${fullWidth ? 'w-full' : ''}`}
  {disabled}
  on:click
>
  {#if loading}
    <svg class="animate-spin -ml-1 mr-2 h-4 w-4 text-white inline" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
      <circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4"></circle>
      <path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
    </svg>
  {/if}
  
  {#if icon && !loading}
    <i class={`${icon} mr-2`}></i>
  {/if}
  
  <slot></slot>
</button>