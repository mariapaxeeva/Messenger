using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using DigiTalk.Models;
using DigiTalk.ViewModels;

namespace DigiTalk.Views
{
    public partial class LoginDialog : Window
    {
        public LoginDialog()
        {
            InitializeComponent();
            DataContext = new LoginDialogViewModel();
#if DEBUG
            this.AttachDevTools();
#endif
        }
        private void InitializeComponent()
        {
            AvaloniaXamlLoader.Load(this);
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            if (DataContext is LoginDialogViewModel vm)
            {
                Close(new LoginDialogResult
                {
                    Result = true,
                    Username = vm.Username,
                    Password = vm.Password,
                    IsRegister = vm.IsRegister
                });
            }
            else
            {
                Close(new LoginDialogResult { Result = false });
            }
        }

        private void CancelButton_Click(object sender, RoutedEventArgs e)
        {
            Close(new LoginDialogResult { Result = false });
        }
    }
}