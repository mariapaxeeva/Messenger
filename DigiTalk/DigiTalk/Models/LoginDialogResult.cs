// Запрос на вход в мессенджер

namespace DigiTalk.Models
{
    public class LoginDialogResult
    {
        public bool Result { get; set; } // true - успех, false - ошибка
        public string? Username { get; set; } // Имя пользователя
        public string? Password { get; set; } // Пароль
        public bool IsRegister { get; set; } // true - запрос является регистрацией, false - авторизацией
    }
}