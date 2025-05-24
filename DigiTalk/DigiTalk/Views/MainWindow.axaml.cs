using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using DigiTalk.ViewModels;
using System;

namespace DigiTalk.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();

#if DEBUG
            this.AttachDevTools();
#endif
        }

        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void OnMessageKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key == Key.Tab && DataContext is MainViewModel vm)
            {
                vm.SendMessageCommand.Execute().Subscribe();
                e.Handled = true;
            }
        }
    }
}
