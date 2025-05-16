using DigiTalk.Views;
using ReactiveUI;

namespace DigiTalk.ViewModels
{
    public class LoginDialogViewModel : ViewModelBase
    {
        private string username;
        public string Username
        {
            get => username;
            set => this.RaiseAndSetIfChanged(ref username, value);
        }

        private string password;
        public string Password
        {
            get => password;
            set => this.RaiseAndSetIfChanged(ref password, value);
        }

        private bool isRegister;
        public bool IsRegister
        {
            get => isRegister;
            set => this.RaiseAndSetIfChanged(ref isRegister, value);
        }
    }
}